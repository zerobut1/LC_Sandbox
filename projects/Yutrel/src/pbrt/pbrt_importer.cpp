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
#include "shapes/sphere.h"
#include "spectrum/hero.h"
#include "surfaces/coated_diffuse.h"
#include "surfaces/diffuse.h"
#include "textures/constant.h"
#include "textures/image.h"
#include "textures/scale.h"

namespace Yutrel
{
static_assert(ShapeDesc::sphere_default_subdivision == Sphere::default_subdivision);
static_assert(ShapeDesc::sphere_max_subdivision == Sphere::max_subdivision);

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

[[nodiscard]] bool is_png_path(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    for (auto& c : ext)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext == ".png";
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
    SceneSpecBuilder builder;

    luisa::unordered_map<luisa::string, const TextureDesc*> texture_declarations;
    texture_declarations.reserve(scene.textures.size());
    for (auto&& texture : scene.textures)
    {
        texture_declarations.emplace(texture.name, &texture);
    }

    for (auto&& texture : scene.textures)
    {
        auto meta = SpecMeta{.name = texture.name, .source = texture.source};
        if (texture.type == TextureDesc::Type::ImageMap)
        {
            auto path = resolve_relative_to_scene(texture.source.file, texture.filename);
            auto encoding = texture.value_type == TextureDesc::ValueType::Float
                                ? Texture::Encoding::LINEAR
                                : is_png_path(path) ? Texture::Encoding::SRGB : Texture::Encoding::LINEAR;
            auto sampler = texture.filter == TextureDesc::Filter::Point
                               ? TextureSampler::point_repeat()
                               : TextureSampler::linear_point_repeat();
            (void)builder.add_texture<ImageTextureSpec>(
                std::move(meta),
                std::move(path),
                sampler,
                encoding,
                texture.uv_scale);
            continue;
        }
        if (texture.type == TextureDesc::Type::Constant)
        {
            (void)builder.add_texture<ConstantTextureSpec>(
                std::move(meta), make_float4(texture.constant_value));
            continue;
        }
        if (texture.type == TextureDesc::Type::Scale)
        {
            auto base_iter = texture_declarations.find(texture.tex);
            if (base_iter == texture_declarations.end())
            {
                fail(luisa::format(
                    "PBRT scale texture '{}' references unknown base texture '{}' at {}.",
                    texture.name, texture.tex, format_source_location(texture.source)));
            }
            if (base_iter->second->value_type != TextureDesc::ValueType::Float)
            {
                fail(luisa::format(
                    "PBRT scale texture '{}' requires '{}' to be a float texture at {}.",
                    texture.name, texture.tex, format_source_location(texture.source)));
            }
            auto scale_iter = texture_declarations.find(texture.scale);
            if (scale_iter == texture_declarations.end())
            {
                fail(luisa::format(
                    "PBRT scale texture '{}' references unknown scale texture '{}' at {}.",
                    texture.name, texture.scale, format_source_location(texture.source)));
            }
            auto scale = scale_iter->second;
            if (scale->value_type != TextureDesc::ValueType::Float ||
                scale->type != TextureDesc::Type::Constant)
            {
                fail(luisa::format(
                    "PBRT scale texture '{}' requires '{}' to be a float constant texture; dynamic multiplication is unsupported at {}.",
                    texture.name, texture.scale, format_source_location(texture.source)));
            }
            auto base = builder.reference_texture(texture.tex, texture.source);
            (void)builder.add_texture<ScaleTextureSpec>(
                std::move(meta), base, make_float4(scale->constant_value), make_float4(0.0f));
            continue;
        }
        fail(luisa::format(
            "Unsupported PBRT texture '{}' at {}.",
            texture.name, format_source_location(texture.source)));
    }

    auto make_material_surface = [&](const MaterialDesc& material, luisa::string_view name) -> SurfaceRef
    {
        auto add_constant_texture = [&](luisa::string_view parameter, float4 value) -> TextureRef
        {
            if (name.empty())
            {
                return builder.add_anonymous_texture<ConstantTextureSpec>(material.source, value);
            }
            return builder.add_texture<ConstantTextureSpec>(
                SpecMeta{.name = luisa::format("{}::{}", name, parameter), .source = material.source},
                value);
        };

        auto reflectance = [&]() -> TextureRef
        {
            if (material.reflectance_texture)
            {
                return builder.reference_texture(*material.reflectance_texture, material.source);
            }
            return add_constant_texture("reflectance", make_float4(
                material.reflectance.x,
                material.reflectance.y,
                material.reflectance.z,
                1.0f));
        }();

        if (material.type == MaterialDesc::Type::Diffuse)
        {
            if (name.empty())
            {
                return builder.add_anonymous_surface<DiffuseSurfaceSpec>(material.source, reflectance, true);
            }
            return builder.add_surface<DiffuseSurfaceSpec>(
                SpecMeta{.name = luisa::string{name}, .source = material.source},
                reflectance,
                true);
        }

        auto roughness = add_constant_texture("roughness", make_float4(material.roughness));
        CoatedDiffuseSurfaceParams params{
            .reflectance = reflectance,
            .roughness   = roughness,
        };
        if (name.empty())
        {
            return builder.add_anonymous_surface<CoatedDiffuseSurfaceSpec>(
                material.source, std::move(params));
        }
        return builder.add_surface<CoatedDiffuseSurfaceSpec>(
            SpecMeta{.name = luisa::string{name}, .source = material.source},
            std::move(params));
    };

    for (auto& [name, material] : scene.named_materials)
    {
        (void)make_material_surface(material, name);
    }

    luisa::vector<SurfaceRef> inline_surface_refs;
    inline_surface_refs.reserve(scene.materials.size());
    for (auto& material : scene.materials)
    {
        inline_surface_refs.emplace_back(make_material_surface(material, {}));
    }

    luisa::optional<SurfaceRef> default_surface;
    auto get_default_surface = [&](const SourceLocation& source) -> SurfaceRef
    {
        if (!default_surface)
        {
            auto texture = builder.add_anonymous_texture<ConstantTextureSpec>(source, make_float4(0.5f, 0.5f, 0.5f, 1.0f));
            default_surface = builder.add_anonymous_surface<DiffuseSurfaceSpec>(source, texture, true);
        }
        return *default_surface;
    };

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
                return builder.add_shape<SphereShapeSpec>(
                    SpecMeta{.name = luisa::format("sphere_{}", shape_index), .source = shape.source},
                    shape.radius,
                    shape.sphere_subdivision);
            }
            fail("Unsupported PBRT shape type.");
        }();
        auto surface = [&]() -> SurfaceRef
        {
            auto has_named  = !shape.material.named.empty();
            auto has_inline = shape.material.inline_index.has_value();
            if (has_named && has_inline)
            {
                fail(luisa::format("PBRT shape has both named and inline material bindings at {}.", format_source_location(shape.source)));
            }
            if (has_named)
            {
                return builder.reference_surface(shape.material.named, shape.source);
            }
            if (has_inline)
            {
                auto index = *shape.material.inline_index;
                if (index >= inline_surface_refs.size())
                {
                    fail(luisa::format(
                        "PBRT shape references out-of-range inline material {} at {}.",
                        index,
                        format_source_location(shape.source)));
                }
                return inline_surface_refs[index];
            }
            return get_default_surface(shape.source);
        }();
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
    auto shutter_span  = make_float2(scene.camera.shutter_open, scene.camera.shutter_close);
    auto exposure_time = shutter_span.y - shutter_span.x;
    if (!std::isfinite(exposure_time) || exposure_time <= 0.0f)
    {
        fail("PBRT shutterclose must be greater than shutteropen.");
    }
    if (!std::isfinite(scene.film.iso) || scene.film.iso <= 0.0f)
    {
        fail("PBRT Film ISO must be finite and positive.");
    }
    auto imaging_ratio = exposure_time * scene.film.iso / 100.0f;
    if (!std::isfinite(imaging_ratio))
    {
        fail("PBRT Film exposure ratio is not finite.");
    }
    auto camera = builder.add_camera<PinholeCameraSpec>(
        SpecMeta{.name = "pbrt_camera", .source = scene.camera.source},
        basis.position,
        basis.position + basis.forward,
        basis.up,
        shutter_span,
        0u,
        scene.camera.fov);
    auto film = builder.add_film<RGBFilmSpec>(
        SpecMeta{.name = "pbrt_film", .source = scene.film.source},
        scene.film.resolution,
        false,
        std::move(filename),
        imaging_ratio);
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
