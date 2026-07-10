#include "pbrt_scene_compiler.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace Yutrel
{
namespace
{

using Matrix4 = std::array<float, 16u>;

[[noreturn]] void fail(luisa::string message)
{
    throw std::runtime_error{message.c_str()};
}

[[nodiscard]] float& at(Matrix4& m, uint row, uint column) noexcept
{
    return m[row * 4u + column];
}

[[nodiscard]] float at(const Matrix4& m, uint row, uint column) noexcept
{
    return m[row * 4u + column];
}

[[nodiscard]] Matrix4 transpose(const std::array<float, 16u>& raw) noexcept
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
    Matrix4 inv{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};

    for (auto column = 0u; column < 4u; column++)
    {
        auto pivot = column;
        auto pivot_abs = std::abs(at(m, pivot, column));
        for (auto row = column + 1u; row < 4u; row++)
        {
            auto candidate_abs = std::abs(at(m, row, column));
            if (candidate_abs > pivot_abs)
            {
                pivot = row;
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

[[nodiscard]] CameraBasis pbrt_camera_transform_to_yutrel(const std::array<float, 16u>& raw)
{
    auto camera_from_world = transpose(raw);
    auto world_from_camera = inverse(camera_from_world);
    return CameraBasis{
        .position = transform_point(world_from_camera, make_float3(0.0f)),
        .forward = normalize_host(transform_vector(world_from_camera, make_float3(0.0f, 0.0f, 1.0f))),
        .up = normalize_host(transform_vector(world_from_camera, make_float3(0.0f, 1.0f, 0.0f))),
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

[[nodiscard]] std::filesystem::path resolve_relative_to_scene(
    const std::filesystem::path& scene_path,
    const std::filesystem::path& path)
{
    if (path.empty())
    {
        return "render.exr";
    }
    if (path.is_absolute())
    {
        return path;
    }
    auto base = scene_path.empty() ? std::filesystem::current_path() : scene_path.parent_path();
    return std::filesystem::absolute(base / path);
}

[[nodiscard]] Filter::CreateInfo compile_filter(const FilterDesc& desc)
{
    if (std::abs(desc.radius.x - desc.radius.y) > 1e-6f)
    {
        fail(luisa::format("PBRT pixel filter expects equal x/y radii for now, got {} and {}.",
                           desc.radius.x, desc.radius.y));
    }
    auto type = Filter::Type::Triangle;
    switch (desc.type)
    {
    case FilterDesc::Type::Triangle:
        type = Filter::Type::Triangle;
        break;
    case FilterDesc::Type::Gaussian:
        type = Filter::Type::Gaussian;
        break;
    default:
        fail("Unsupported PBRT pixel filter.");
    }
    return Filter::CreateInfo{
        .type = type,
        .radius = desc.radius.x,
    };
}

[[nodiscard]] Camera::CreateInfo compile_camera(const SceneDescription& desc)
{
    if (desc.camera.type != CameraDesc::Type::Perspective)
    {
        fail("Unsupported PBRT camera type.");
    }
    auto basis = pbrt_camera_transform_to_yutrel(desc.camera.pbrt_transform);
    auto filename = desc.film.filename.empty() ?
                        std::filesystem::path{"render.exr"} :
                        resolve_relative_to_scene(desc.source_path, desc.film.filename);
    if (!is_exr_path(filename))
    {
        fail(luisa::format("Yutrel only supports EXR film output, got '{}'.", filename.string()));
    }
    return Camera::CreateInfo{
        .type = Camera::Type::pinhole,
        .film_info = {
            .resolution = desc.film.resolution,
            .display_hdr = false,
            .filename = std::move(filename),
        },
        .filter_info = compile_filter(desc.filter),
        .spp = desc.sampler.pixel_samples,
        .position = basis.position,
        .lookat = basis.position + basis.forward,
        .up = basis.up,
        .fov = desc.camera.fov,
    };
}

[[nodiscard]] Texture::CreateInfo constant_texture(float3 rgb) noexcept
{
    return Texture::CreateInfo{
        .type = Texture::Type::constant,
        .v = make_float4(rgb.x, rgb.y, rgb.z, 1.0f),
    };
}

[[nodiscard]] luisa::vector<Shape::CreateInfo> compile_shapes(const SceneDescription& desc)
{
    luisa::vector<Shape::CreateInfo> shapes;
    shapes.reserve(desc.shapes.size());
    for (auto&& shape_desc : desc.shapes)
    {
        if (shape_desc.mesh_index >= desc.meshes.size())
        {
            fail("PBRT shape references an out-of-range mesh.");
        }
        auto material_iter = desc.named_materials.find(shape_desc.material_name);
        if (material_iter == desc.named_materials.end())
        {
            fail(luisa::format("PBRT shape references undefined material '{}'.", shape_desc.material_name));
        }
        auto&& mesh = desc.meshes[shape_desc.mesh_index];
        auto&& material = material_iter->second;
        if (material.type != MaterialDesc::Type::Diffuse)
        {
            fail("Unsupported PBRT material type.");
        }

        Shape::CreateInfo shape{
            .type = Shape::Type::inline_mesh,
            .positions = mesh.positions,
            .normals = mesh.normals,
            .uvs = mesh.uvs,
            .indices = mesh.indices,
            .surface_info = {
                .type = Surface::Type::diffuse,
                .reflectance = constant_texture(material.reflectance),
                .two_sided = true,
            },
        };
        if (shape_desc.area_light)
        {
            if (shape_desc.area_light->type != AreaLightDesc::Type::Diffuse)
            {
                fail("Unsupported PBRT area light type.");
            }
            shape.light_info = Light::CreateInfo{
                .type = Light::Type::diffuse,
                .emission = constant_texture(shape_desc.area_light->emission),
            };
        }
        shapes.emplace_back(std::move(shape));
    }
    return shapes;
}

} // namespace

Scene::CreateInfo PbrtSceneCompiler::compile(SceneDescription desc)
{
    if (desc.integrator.type != IntegratorDesc::Type::Path)
    {
        fail("Unsupported PBRT integrator type.");
    }
    if (desc.sampler.type != SamplerDesc::Type::Independent)
    {
        fail("Unsupported PBRT sampler type.");
    }
    if (desc.film.type != FilmDesc::Type::RGB)
    {
        fail("Unsupported PBRT film type.");
    }
    return Scene::CreateInfo{
        .spectrum_info = {
            .type = Spectrum::Type::HeroWavelength,
        },
        .integrator_info = {
            .max_depth = desc.integrator.max_depth,
        },
        .camera_info = compile_camera(desc),
        .shape_infos = compile_shapes(desc),
    };
}

} // namespace Yutrel
