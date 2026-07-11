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
#include "shapes/mesh.h"
#include "spectrum/hero.h"
#include "surfaces/diffuse.h"
#include "textures/constant.h"

namespace Yutrel
{
namespace
{

[[noreturn]] void fail(luisa::string message)
{
    throw std::runtime_error{message.c_str()};
}

[[nodiscard]] float& at(Matrix4& m, uint32_t row, uint32_t column) noexcept { return m[row * 4u + column]; }
[[nodiscard]] float at(const Matrix4& m, uint32_t row, uint32_t column) noexcept { return m[row * 4u + column]; }

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
    auto world_from_camera = inverse(raw);
    return CameraBasis{
        .position = transform_point(world_from_camera, make_float3(0.0f)),
        .forward  = normalize_host(transform_vector(world_from_camera, make_float3(0.0f, 0.0f, 1.0f))),
        .up       = normalize_host(transform_vector(world_from_camera, make_float3(0.0f, 1.0f, 0.0f))),
    };
}

[[nodiscard]] float4x4 instance_transform(const std::array<float, 16u>& raw) noexcept
{
    return make_float4x4(
        make_float4(raw[0u], raw[4u], raw[8u], raw[12u]),
        make_float4(raw[1u], raw[5u], raw[9u], raw[13u]),
        make_float4(raw[2u], raw[6u], raw[10u], raw[14u]),
        make_float4(raw[3u], raw[7u], raw[11u], raw[15u]));
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
        fail(luisa::format("PBRT Importer does not implement sampler 'halton' at {}.", format_source_location(scene.sampler.source)));
    }
    if (scene.film.type != FilmDesc::Type::RGB)
    {
        fail("Unsupported PBRT film type.");
    }
    if (scene.camera.type != CameraDesc::Type::Perspective)
    {
        fail("Unsupported PBRT camera type.");
    }
    if (!scene.textures.empty())
    {
        fail(luisa::format("PBRT Importer does not implement Texture at {}.", format_source_location(scene.textures.front().source)));
    }
    if (!scene.materials.empty())
    {
        fail(luisa::format("PBRT Importer does not implement inline Material at {}.", format_source_location(scene.materials.front().source)));
    }

    SceneSpecBuilder builder;
    for (auto& [name, material] : scene.named_materials)
    {
        if (material.type != MaterialDesc::Type::Diffuse)
        {
            fail(luisa::format("Unsupported PBRT material '{}'.", name));
        }
        auto texture = builder.add_texture<ConstantTextureSpec>(
            SpecMeta{.name = luisa::format("{}::reflectance", name), .source = material.source},
            make_float4(material.reflectance.x, material.reflectance.y, material.reflectance.z, 1.0f));
        (void)builder.add_surface<DiffuseSurfaceSpec>(
            SpecMeta{.name = name, .source = material.source},
            texture,
            true);
    }

    luisa::vector<ShapeRef> inline_mesh_refs;
    inline_mesh_refs.reserve(scene.meshes.size());
    for (auto& mesh : scene.meshes)
    {
        inline_mesh_refs.emplace_back(builder.add_shape<InlineMeshShapeSpec>(
            SpecMeta{.name = luisa::format("mesh_{}", inline_mesh_refs.size()), .source = mesh.source},
            std::move(mesh.positions),
            std::move(mesh.normals),
            std::move(mesh.uvs),
            std::move(mesh.indices)));
    }

    for (auto shape_index = 0u; shape_index < scene.shapes.size(); shape_index++)
    {
        auto& shape = scene.shapes[shape_index];
        auto shape_ref = [&]() -> ShapeRef
        {
            switch (shape.type)
            {
            case ShapeDesc::Type::TriangleMesh:
                if (!shape.mesh_index || *shape.mesh_index >= inline_mesh_refs.size())
                {
                    fail(luisa::format("PBRT shape references an out-of-range mesh at {}.", format_source_location(shape.source)));
                }
                return inline_mesh_refs[*shape.mesh_index];
            case ShapeDesc::Type::PlyMesh:
                if (!shape.filename || shape.filename->empty())
                {
                    fail(luisa::format("PBRT plymesh has no filename at {}.", format_source_location(shape.source)));
                }
                return builder.add_shape<MeshShapeSpec>(
                    SpecMeta{.name = luisa::format("plymesh_{}", shape_index), .source = shape.source},
                    resolve_relative_to_scene(scene.source_path, *shape.filename));
            case ShapeDesc::Type::Sphere:
                fail(luisa::format("PBRT Importer does not implement Shape 'sphere' at {}.", format_source_location(shape.source)));
            }
            fail("Unsupported PBRT shape type.");
        }();
        if (shape.material.named.empty())
        {
            fail(luisa::format("PBRT Importer does not implement anonymous/default material binding at {}.", format_source_location(shape.source)));
        }
        auto surface = builder.reference_surface(shape.material.named, shape.source);
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
            .source    = shape.source,
            .shape     = shape_ref,
            .surface   = surface,
            .light     = light,
            .transform = instance_transform(shape.pbrt_transform),
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
