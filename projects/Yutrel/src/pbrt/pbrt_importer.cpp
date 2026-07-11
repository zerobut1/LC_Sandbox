#include "pbrt_importer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "base/film.h"
#include "base/integrator.h"
#include "cameras/pinhole.h"
#include "filters/gaussian.h"
#include "filters/triangle.h"
#include "lights/diffuse.h"
#include "samplers/independent.h"
#include "scene/scene_spec_builder.h"
#include "shapes/inline_mesh.h"
#include "spectrum/hero.h"
#include "surfaces/diffuse.h"
#include "textures/constant.h"

namespace Yutrel
{
namespace
{

using Matrix4 = std::array<float, 16u>;

[[noreturn]] void fail(luisa::string message)
{
    throw std::runtime_error{message.c_str()};
}

[[nodiscard]] float& at(Matrix4& m, uint32_t row, uint32_t column) noexcept { return m[row * 4u + column]; }
[[nodiscard]] float at(const Matrix4& m, uint32_t row, uint32_t column) noexcept { return m[row * 4u + column]; }

[[nodiscard]] Matrix4 transpose(const Matrix4& raw) noexcept
{
    Matrix4 result{};
    for (auto row = 0u; row < 4u; row++)
    {
        for (auto column = 0u; column < 4u; column++)
        {
            at(result, row, column) = raw[column * 4u + row];
        }
    }
    return result;
}

[[nodiscard]] Matrix4 inverse(Matrix4 m)
{
    Matrix4 inv{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    for (auto column = 0u; column < 4u; column++)
    {
        auto pivot     = column;
        auto pivot_abs = std::abs(at(m, pivot, column));
        for (auto row = column + 1u; row < 4u; row++)
        {
            auto candidate_abs = std::abs(at(m, row, column));
            if (candidate_abs > pivot_abs)
            {
                pivot     = row;
                pivot_abs = candidate_abs;
            }
        }
        if (pivot_abs < 1e-8f)
        {
            fail("PBRT camera transform is singular.");
        }
        if (pivot != column)
        {
            for (auto i = 0u; i < 4u; i++)
            {
                std::swap(at(m, column, i), at(m, pivot, i));
                std::swap(at(inv, column, i), at(inv, pivot, i));
            }
        }
        auto inv_pivot = 1.0f / at(m, column, column);
        for (auto i = 0u; i < 4u; i++)
        {
            at(m, column, i) *= inv_pivot;
            at(inv, column, i) *= inv_pivot;
        }
        for (auto row = 0u; row < 4u; row++)
        {
            if (row == column)
            {
                continue;
            }
            auto factor = at(m, row, column);
            for (auto i = 0u; i < 4u; i++)
            {
                at(m, row, i) -= factor * at(m, column, i);
                at(inv, row, i) -= factor * at(inv, column, i);
            }
        }
    }
    return inv;
}

[[nodiscard]] float3 transform_point(const Matrix4& m, float3 p) noexcept
{
    auto x = at(m, 0u, 0u) * p.x + at(m, 0u, 1u) * p.y + at(m, 0u, 2u) * p.z + at(m, 0u, 3u);
    auto y = at(m, 1u, 0u) * p.x + at(m, 1u, 1u) * p.y + at(m, 1u, 2u) * p.z + at(m, 1u, 3u);
    auto z = at(m, 2u, 0u) * p.x + at(m, 2u, 1u) * p.y + at(m, 2u, 2u) * p.z + at(m, 2u, 3u);
    auto w = at(m, 3u, 0u) * p.x + at(m, 3u, 1u) * p.y + at(m, 3u, 2u) * p.z + at(m, 3u, 3u);
    if (std::abs(w) > 1e-8f && std::abs(w - 1.0f) > 1e-8f)
    {
        x /= w;
        y /= w;
        z /= w;
    }
    return make_float3(x, y, z);
}

[[nodiscard]] float3 transform_vector(const Matrix4& m, float3 v) noexcept
{
    return make_float3(
        at(m, 0u, 0u) * v.x + at(m, 0u, 1u) * v.y + at(m, 0u, 2u) * v.z,
        at(m, 1u, 0u) * v.x + at(m, 1u, 1u) * v.y + at(m, 1u, 2u) * v.z,
        at(m, 2u, 0u) * v.x + at(m, 2u, 1u) * v.y + at(m, 2u, 2u) * v.z);
}

[[nodiscard]] float3 normalize_host(float3 v)
{
    auto len2 = v.x * v.x + v.y * v.y + v.z * v.z;
    if (len2 < 1e-16f)
    {
        fail("PBRT camera transform produced a zero-length direction.");
    }
    auto inv_len = 1.0f / std::sqrt(len2);
    return make_float3(v.x * inv_len, v.y * inv_len, v.z * inv_len);
}

struct CameraBasis
{
    float3 position;
    float3 forward;
    float3 up;
};

[[nodiscard]] CameraBasis camera_basis(const std::array<float, 16u>& raw)
{
    auto camera_from_world = transpose(raw);
    auto world_from_camera = inverse(camera_from_world);
    return CameraBasis{
        .position = transform_point(world_from_camera, make_float3(0.0f)),
        .forward  = normalize_host(transform_vector(world_from_camera, make_float3(0.0f, 0.0f, 1.0f))),
        .up       = normalize_host(transform_vector(world_from_camera, make_float3(0.0f, 1.0f, 0.0f))),
    };
}

[[nodiscard]] bool is_exr_path(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    for (auto& c : ext)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext == ".exr";
}

[[nodiscard]] std::filesystem::path resolve_relative_to_scene(const std::filesystem::path& scene_path, const std::filesystem::path& path)
{
    if (path.empty())
    {
        return "render.exr";
    }
    if (path.is_absolute())
    {
        return path;
    }
    return std::filesystem::absolute(scene_path.parent_path() / path);
}

} // namespace

SceneSpec PbrtImporter::import(PbrtScene scene)
{
    if (scene.integrator.type != IntegratorDesc::Type::Path)
    {
        fail("Unsupported PBRT integrator type.");
    }
    if (scene.sampler.type != SamplerDesc::Type::Independent)
    {
        fail("Unsupported PBRT sampler type.");
    }
    if (scene.film.type != FilmDesc::Type::RGB)
    {
        fail("Unsupported PBRT film type.");
    }
    if (scene.camera.type != CameraDesc::Type::Perspective)
    {
        fail("Unsupported PBRT camera type.");
    }

    SceneSpecBuilder builder;
    luisa::unordered_map<luisa::string, SurfaceRef> material_surfaces;
    for (auto& [name, material] : scene.named_materials)
    {
        if (material.type != MaterialDesc::Type::Diffuse)
        {
            fail(luisa::format("Unsupported PBRT material '{}'.", name));
        }
        auto texture = builder.add_texture<ConstantTextureSpec>(
            SpecMeta{.name = luisa::format("{}::reflectance", name), .source = material.source},
            make_float4(material.reflectance.x, material.reflectance.y, material.reflectance.z, 1.0f));
        auto surface = builder.add_surface<DiffuseSurfaceSpec>(
            SpecMeta{.name = name, .source = material.source},
            texture,
            true);
        material_surfaces.emplace(name, surface);
    }

    luisa::vector<ShapeRef> shape_refs;
    shape_refs.reserve(scene.meshes.size());
    for (auto& mesh : scene.meshes)
    {
        shape_refs.emplace_back(builder.add_shape<InlineMeshShapeSpec>(
            SpecMeta{.name = luisa::format("mesh_{}", shape_refs.size()), .source = mesh.source},
            std::move(mesh.positions),
            std::move(mesh.normals),
            std::move(mesh.uvs),
            std::move(mesh.indices)));
    }

    for (auto& shape : scene.shapes)
    {
        if (shape.mesh_index >= shape_refs.size())
        {
            fail(luisa::format("PBRT shape references an out-of-range mesh at {}.", format_source_location(shape.source)));
        }
        auto material_iter = material_surfaces.find(shape.material_name);
        if (material_iter == material_surfaces.end())
        {
            fail(luisa::format("PBRT shape references undefined material '{}' at {}.", shape.material_name, format_source_location(shape.source)));
        }
        luisa::optional<LightRef> light;
        if (shape.area_light)
        {
            if (shape.area_light->type != AreaLightDesc::Type::Diffuse)
            {
                fail("Unsupported PBRT area light type.");
            }
            auto emission = builder.add_anonymous_texture<ConstantTextureSpec>(
                shape.area_light->source,
                make_float4(shape.area_light->emission.x, shape.area_light->emission.y, shape.area_light->emission.z, 1.0f));
            light = builder.add_anonymous_light<DiffuseLightSpec>(shape.area_light->source, emission, 1.0f, false);
        }
        builder.add_instance(ShapeInstanceSpec{
            .source  = shape.source,
            .shape   = shape_refs[shape.mesh_index],
            .surface = material_iter->second,
            .light   = light,
        });
    }

    auto basis    = camera_basis(scene.camera.pbrt_transform);
    auto filename = resolve_relative_to_scene(scene.source_path, scene.film.filename);
    if (!is_exr_path(filename))
    {
        fail(luisa::format("Yutrel only supports EXR film output, got '{}'.", filename.string()));
    }
    auto camera = builder.add_camera<PinholeCameraSpec>(
        SpecMeta{.name = "pbrt_camera", .source = scene.camera.source},
        basis.position,
        basis.position + basis.forward,
        basis.up,
        make_float2(0.0f),
        0u,
        scene.camera.fov);
    auto film = builder.add_film<RGBFilmSpec>(
        SpecMeta{.name = "pbrt_film", .source = scene.film.source},
        scene.film.resolution,
        false,
        std::move(filename));
    if (std::abs(scene.filter.radius.x - scene.filter.radius.y) > 1e-6f)
    {
        fail("PBRT pixel filter expects equal x/y radii for now.");
    }
    auto filter = [&]() -> FilterRef
    {
        switch (scene.filter.type)
        {
        case FilterDesc::Type::Triangle:
            return builder.add_filter<TriangleFilterSpec>(SpecMeta{.name = "pbrt_filter", .source = scene.filter.source}, scene.filter.radius.x);
        case FilterDesc::Type::Gaussian:
            return builder.add_filter<GaussianFilterSpec>(SpecMeta{.name = "pbrt_filter", .source = scene.filter.source}, scene.filter.radius.x);
        }
        fail("Unsupported PBRT pixel filter.");
    }();
    auto spectrum   = builder.add_spectrum<HeroWavelengthSpectrumSpec>(SpecMeta{.name = "pbrt_spectrum", .source = SourceLocation{scene.source_path}});
    auto sampler    = builder.add_sampler<IndependentSamplerSpec>(SpecMeta{.name = "pbrt_sampler", .source = scene.sampler.source}, scene.sampler.pixel_samples, 20120712u);
    auto integrator = builder.add_integrator<PathIntegratorSpec>(SpecMeta{.name = "pbrt_integrator", .source = scene.integrator.source}, scene.integrator.max_depth, 0u, 0.95f);
    builder.set_render(RenderSpec{
        .spectrum   = spectrum,
        .camera     = camera,
        .film       = film,
        .filter     = filter,
        .sampler    = sampler,
        .integrator = integrator,
    });
    return builder.finish();
}

} // namespace Yutrel
