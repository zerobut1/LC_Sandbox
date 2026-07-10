#include "pbrt/pbrt_scene_compiler.h"
#include "pbrt/pbrt_scene_loader.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

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

void require_float4(float4 v, float x, float y, float z, float w, const char* message)
{
    require_close(v.x, x, message);
    require_close(v.y, y, message);
    require_close(v.z, z, message);
    require_close(v.w, w, message);
}

void expect_compile_failure(SceneDescription desc, const char* message_substring)
{
    try
    {
        (void)PbrtSceneCompiler::compile(std::move(desc));
    }
    catch (const std::exception& e)
    {
        auto message = std::string{e.what()};
        require(message.find(message_substring) != std::string::npos, "unexpected compiler error");
        return;
    }
    throw std::runtime_error{"expected PBRT scene compiler failure"};
}

} // namespace

int main() try
{
    auto scene_path = std::filesystem::current_path() / "projects/Yutrel/scene/cornell-box/scene-v4.pbrt";
    if (!std::filesystem::exists(scene_path))
    {
        scene_path = std::filesystem::current_path() / "scene/cornell-box/scene-v4.pbrt";
    }

    auto desc = PbrtSceneLoader::load(scene_path);
    desc.film.filename = "cornell-box.exr";
    auto info = PbrtSceneCompiler::compile(std::move(desc));

    auto gaussian_desc = PbrtSceneLoader::load(scene_path.parent_path() / "scene-v5.pbrt");
    auto gaussian_info = PbrtSceneCompiler::compile(std::move(gaussian_desc));
    require(gaussian_info.integrator_info.max_depth == 10u, "Gaussian scene integrator max depth");
    require(gaussian_info.camera_info.filter_info.type == Filter::Type::Gaussian, "Gaussian scene filter type");
    require_close(gaussian_info.camera_info.filter_info.radius, 1.0f, "Gaussian scene filter radius");
    require(gaussian_info.camera_info.spp == 65536u, "Gaussian scene spp");
    require(gaussian_info.camera_info.film_info.filename == "render.exr", "Gaussian scene default film filename");

    auto uppercase_desc = PbrtSceneLoader::load(scene_path);
    uppercase_desc.film.filename = "cornell-box.EXR";
    auto uppercase_info = PbrtSceneCompiler::compile(std::move(uppercase_desc));
    require(uppercase_info.camera_info.film_info.filename == scene_path.parent_path() / "cornell-box.EXR",
            "uppercase EXR film filename");

    for (auto filename : {"cornell-box.png", "cornell-box.hdr", "cornell-box"})
    {
        auto invalid_desc = PbrtSceneLoader::load(scene_path);
        invalid_desc.film.filename = filename;
        expect_compile_failure(std::move(invalid_desc), "only supports EXR film output");
    }

    require(info.spectrum_info.type == Spectrum::Type::HeroWavelength, "spectrum type");
    require(info.integrator_info.max_depth == 65u, "integrator max depth");

    auto&& camera = info.camera_info;
    require(camera.type == Camera::Type::pinhole, "camera type");
    require(camera.film_info.resolution.x == 1024u && camera.film_info.resolution.y == 1024u, "film resolution");
    require(!camera.film_info.display_hdr, "film display hdr");
    require(camera.film_info.filename == scene_path.parent_path() / "cornell-box.exr", "film filename");
    require(camera.filter_info.type == Filter::Type::Triangle, "filter type");
    require_close(camera.filter_info.radius, 1.0f, "filter radius");
    require(camera.spp == 64u, "camera spp");
    require_close(camera.fov, 19.5f, "camera fov");
    require_float3(camera.position, 0.0f, 1.0f, 6.8f, "camera position");
    require_float3(camera.lookat - camera.position, 0.0f, 0.0f, -1.0f, "camera forward");
    require_float3(camera.up, 0.0f, 1.0f, 0.0f, "camera up");

    require(info.shape_infos.size() == 8u, "shape count");
    uint triangle_count = 0u;
    bool saw_left_wall = false;
    bool saw_right_wall = false;
    bool saw_light = false;
    for (auto&& shape : info.shape_infos)
    {
        require(shape.type == Shape::Type::inline_mesh, "shape type");
        triangle_count += static_cast<uint>(shape.indices.size());
        if (shape.surface_info.type == Surface::Type::diffuse)
        {
            require(shape.surface_info.two_sided, "PBRT diffuse surface two-sided");
            auto r = shape.surface_info.reflectance.v;
            if (std::abs(r.x - 0.63f) < 1e-5f)
            {
                require_float4(r, 0.63f, 0.065f, 0.05f, 1.0f, "LeftWall reflectance");
                saw_left_wall = true;
            }
            if (std::abs(r.x - 0.14f) < 1e-5f)
            {
                require_float4(r, 0.14f, 0.45f, 0.091f, 1.0f, "RightWall reflectance");
                saw_right_wall = true;
            }
        }
        if (shape.light_info.type == Light::Type::diffuse)
        {
            require_float4(shape.light_info.emission.v, 17.0f, 12.0f, 4.0f, 1.0f, "area light emission");
            require(!shape.light_info.two_sided, "PBRT area light is single-sided");
            saw_light = true;
        }
    }
    require(triangle_count == 36u, "triangle count");
    require(saw_left_wall, "saw LeftWall");
    require(saw_right_wall, "saw RightWall");
    require(saw_light, "saw area light");

    std::cout << "PBRT scene compiler tests passed.\n";
    return 0;
}
catch (const std::exception& e)
{
    std::cerr << "PBRT scene compiler test failed: " << e.what() << '\n';
    return 1;
}
