#include "pbrt/pbrt_scene_loader.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Yutrel;

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

void require_close(float a, float b, const char* message)
{
    if (std::abs(a - b) > 1e-5f)
    {
        throw std::runtime_error{message};
    }
}

void require_float3(float3 v, float x, float y, float z, const char* message)
{
    require_close(v.x, x, message);
    require_close(v.y, y, message);
    require_close(v.z, z, message);
}

void expect_load_failure(const std::filesystem::path& path, const char* contents, const char* message_substring)
{
    std::ofstream output{path};
    output << contents;
    output.close();

    try
    {
        (void)PbrtSceneLoader::load(path);
    }
    catch (const std::exception& e)
    {
        auto msg = std::string{e.what()};
        if (msg.find(message_substring) == std::string::npos)
        {
            auto s = luisa::format("Expected error containing '{}', got '{}'.", message_substring, msg);
            throw std::runtime_error{s.c_str()};
        }
        return;
    }
    auto s = luisa::format("Expected load failure containing '{}'.", message_substring);
    throw std::runtime_error{s.c_str()};
}

} // namespace

int main()
{
    auto scene_path = std::filesystem::current_path() / "projects/Yutrel/scene/cornell-box/scene-v4.pbrt";
    if (!std::filesystem::exists(scene_path))
    {
        scene_path = std::filesystem::current_path() / "scene/cornell-box/scene-v4.pbrt";
    }

    auto desc = PbrtSceneLoader::load(scene_path);

    require(desc.integrator.type == IntegratorDesc::Type::Path, "integrator type");
    require(desc.integrator.max_depth == 65u, "integrator max depth");
    require(desc.sampler.type == SamplerDesc::Type::Sobol, "sampler type");
    require(desc.sampler.pixel_samples == 64u, "sampler pixel samples");
    require(desc.filter.type == FilterDesc::Type::Triangle, "filter type");
    require_close(desc.filter.radius.x, 1.0f, "filter x radius");
    require_close(desc.filter.radius.y, 1.0f, "filter y radius");
    require(desc.film.type == FilmDesc::Type::RGB, "film type");
    require(desc.film.resolution.x == 1024u && desc.film.resolution.y == 1024u, "film resolution");
    require(desc.film.filename == "cornell-box.png", "film filename");
    require(desc.camera.type == CameraDesc::Type::Perspective, "camera type");
    require_close(desc.camera.fov, 19.5f, "camera fov");

    std::array<float, 16u> expected_transform{
        1.0f, -0.0f, -0.0f, -0.0f,
        -0.0f, 1.0f, -0.0f, -0.0f,
        -0.0f, -0.0f, -1.0f, -0.0f,
        -0.0f, -1.0f, 6.8f, 1.0f};
    for (auto i = 0u; i < 16u; i++)
    {
        require_close(desc.camera.pbrt_transform[i], expected_transform[i], "camera transform");
    }

    require(desc.named_materials.size() == 8u, "named material count");
    require_float3(desc.named_materials.at("LeftWall").reflectance, 0.63f, 0.065f, 0.05f, "LeftWall reflectance");
    require_float3(desc.named_materials.at("RightWall").reflectance, 0.14f, 0.45f, 0.091f, "RightWall reflectance");
    require_float3(desc.named_materials.at("Light").reflectance, 0.0f, 0.0f, 0.0f, "Light reflectance");

    require(desc.meshes.size() == 8u, "mesh count");
    require(desc.shapes.size() == 8u, "shape count");
    uint triangle_count = 0u;
    for (auto&& mesh : desc.meshes)
    {
        triangle_count += static_cast<uint>(mesh.indices.size());
    }
    require(triangle_count == 36u, "triangle count");

    auto&& light_shape = desc.shapes.back();
    require(light_shape.area_light.has_value(), "last shape area light");
    require_float3(light_shape.area_light->emission, 17.0f, 12.0f, 4.0f, "area light emission");

    auto tmp = std::filesystem::temp_directory_path();
    expect_load_failure(tmp / "yutrel_missing_p.pbrt",
                        R"(Integrator "path"
WorldBegin
MakeNamedMaterial "M" "string type" [ "diffuse" ] "rgb reflectance" [ 1 1 1 ]
NamedMaterial "M"
Shape "trianglemesh" "integer indices" [ 0 1 2 ]
)",
                        "missing parameter '\"point3 P\"'");
    expect_load_failure(tmp / "yutrel_bad_indices.pbrt",
                        R"(Integrator "path"
WorldBegin
MakeNamedMaterial "M" "string type" [ "diffuse" ] "rgb reflectance" [ 1 1 1 ]
NamedMaterial "M"
Shape "trianglemesh" "point3 P" [ 0 0 0 1 0 0 0 1 0 ] "normal N" [ 0 0 1 0 0 1 0 0 1 ] "integer indices" [ 0 1 ]
)",
                        "multiple of 3");
    expect_load_failure(tmp / "yutrel_missing_material.pbrt",
                        R"(Integrator "path"
WorldBegin
NamedMaterial "Missing"
)",
                        "undefined material");
    expect_load_failure(tmp / "yutrel_unsupported_command.pbrt",
                        R"(Integrator "path"
WorldBegin
Include "other.pbrt"
)",
                        "unsupported PBRT command");

    std::cout << "PBRT scene loader tests passed.\n";
    return 0;
}
