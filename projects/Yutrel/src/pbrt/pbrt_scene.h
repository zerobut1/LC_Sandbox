#pragma once

#include <array>
#include <filesystem>

#include <luisa/core/basic_types.h>
#include <luisa/core/stl.h>

#include "scene/source_location.h"

namespace Yutrel
{
using namespace luisa;

using Matrix4 = std::array<float, 16u>;

inline constexpr Matrix4 identity_matrix4{
    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

struct RawValue
{
    SourceLocation source;
    luisa::string text;
    bool quoted{};
};

struct RawParameter
{
    SourceLocation source;
    luisa::string type;
    luisa::string name;
    luisa::vector<RawValue> values;
    bool bracketed{};
};

struct CameraDesc
{
    SourceLocation source;
    enum class Type
    {
        Perspective,
    };
    Type type{Type::Perspective};
    float fov{45.0f};
    Matrix4 pbrt_transform{identity_matrix4};
    luisa::vector<RawParameter> parameters;
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
    luisa::vector<RawParameter> parameters;
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
    luisa::vector<RawParameter> parameters;
};

struct SamplerDesc
{
    SourceLocation source;
    enum class Type
    {
        Independent,
        Halton,
    };
    Type type{Type::Independent};
    uint pixel_samples{1u};
    luisa::vector<RawParameter> parameters;
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
    luisa::vector<RawParameter> parameters;
};

struct TextureDesc
{
    SourceLocation source;
    enum class ValueType
    {
        Float,
        Spectrum,
    };
    enum class Type
    {
        ImageMap,
        Constant,
        Scale,
    };
    luisa::string name;
    ValueType value_type{ValueType::Float};
    Type type{Type::Constant};
    std::filesystem::path filename;
    luisa::vector<RawParameter> parameters;
};

struct MaterialDesc
{
    SourceLocation source;
    enum class Type
    {
        Diffuse,
        CoatedDiffuse,
    };
    Type type{Type::Diffuse};
    float3 reflectance{0.0f, 0.0f, 0.0f};
    luisa::optional<luisa::string> reflectance_texture;
    luisa::vector<RawParameter> parameters;
};

struct MaterialBinding
{
    luisa::string named;
    luisa::optional<uint> inline_index;
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
    luisa::vector<RawParameter> parameters;
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
    static constexpr uint sphere_default_subdivision = 4u;
    static constexpr uint sphere_max_subdivision     = 8u;

    SourceLocation source;
    enum class Type
    {
        TriangleMesh,
        PlyMesh,
        Sphere,
    };
    Type type{Type::TriangleMesh};
    luisa::optional<uint> mesh_index;
    luisa::optional<std::filesystem::path> filename;
    float radius{1.0f};
    uint sphere_subdivision{sphere_default_subdivision};
    luisa::vector<RawParameter> parameters;
    MaterialBinding material;
    luisa::optional<AreaLightDesc> area_light;
    Matrix4 pbrt_transform{identity_matrix4};
};

struct PbrtScene
{
    std::filesystem::path source_path;
    CameraDesc camera;
    FilmDesc film;
    IntegratorDesc integrator;
    SamplerDesc sampler;
    FilterDesc filter;
    luisa::vector<TextureDesc> textures;
    luisa::vector<MaterialDesc> materials;
    luisa::unordered_map<luisa::string, MaterialDesc> named_materials;
    luisa::vector<MeshDesc> meshes;
    luisa::vector<ShapeDesc> shapes;
};

} // namespace Yutrel
