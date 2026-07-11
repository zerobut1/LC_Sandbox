#pragma once

#include <array>
#include <filesystem>

#include <luisa/core/basic_types.h>
#include <luisa/core/stl.h>

#include "scene/source_location.h"

namespace Yutrel
{
using namespace luisa;

struct CameraDesc
{
    SourceLocation source;
    enum class Type
    {
        Perspective,
    };
    Type type{Type::Perspective};
    float fov{45.0f};
    std::array<float, 16u> pbrt_transform{
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
};

struct FilmDesc
{
    SourceLocation source;
    enum class Type
    {
        RGB,
    };
    Type type{Type::RGB};
    uint2 resolution{1024u, 1024u};
    std::filesystem::path filename;
};

struct IntegratorDesc
{
    SourceLocation source;
    enum class Type
    {
        Path,
    };
    Type type{Type::Path};
    uint max_depth{10u};
};

struct SamplerDesc
{
    SourceLocation source;
    enum class Type
    {
        Independent,
    };
    Type type{Type::Independent};
    uint pixel_samples{1u};
};

struct FilterDesc
{
    SourceLocation source;
    enum class Type
    {
        Triangle,
        Gaussian,
    };
    Type type{Type::Triangle};
    float2 radius{1.0f, 1.0f};
};

struct MaterialDesc
{
    SourceLocation source;
    enum class Type
    {
        Diffuse,
    };
    Type type{Type::Diffuse};
    float3 reflectance{0.0f, 0.0f, 0.0f};
};

struct AreaLightDesc
{
    SourceLocation source;
    enum class Type
    {
        Diffuse,
    };
    Type type{Type::Diffuse};
    float3 emission{0.0f, 0.0f, 0.0f};
};

struct MeshDesc
{
    SourceLocation source;
    luisa::vector<float3> positions;
    luisa::vector<float3> normals;
    luisa::vector<float2> uvs;
    luisa::vector<uint3> indices;
};

struct ShapeDesc
{
    SourceLocation source;
    uint mesh_index{};
    luisa::string material_name;
    luisa::optional<AreaLightDesc> area_light;
};

struct PbrtScene
{
    std::filesystem::path source_path;
    CameraDesc camera;
    FilmDesc film;
    IntegratorDesc integrator;
    SamplerDesc sampler;
    FilterDesc filter;
    luisa::unordered_map<luisa::string, MaterialDesc> named_materials;
    luisa::vector<MeshDesc> meshes;
    luisa::vector<ShapeDesc> shapes;
};

} // namespace Yutrel
