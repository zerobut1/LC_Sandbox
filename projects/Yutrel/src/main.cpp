#include "base/application.h"

#include <luisa/core/logging.h>

#include <cstdlib>
#include <filesystem>

using namespace Yutrel;

namespace
{

[[noreturn]] void print_usage_and_exit(char const* bin)
{
    LUISA_ERROR("Usage: {} <backend> [scene.pbrt] [--interactive|-i]. <backend>: cuda, dx, metal", bin);
    std::abort();
}

[[nodiscard]] bool is_interactive_arg(luisa::string_view arg) noexcept
{
    return arg == "--interactive" || arg == "-i";
}

[[nodiscard]] bool is_pbrt_scene_path(std::filesystem::path const& path)
{
    return path.extension() == ".pbrt";
}

[[nodiscard]] Scene::CreateInfo make_default_cornell_box_scene_info()
{
    Scene::CreateInfo scene_info{};

    scene_info.spectrum_info = {
        .type = Spectrum::Type::HeroWavelength,
    };
    scene_info.camera_info = {
        .type      = Camera::Type::pinhole,
        .film_info = {
            .resolution = make_uint2(1024u),
            .hdr        = false},
        .filter_info = {.type = Filter::Type::Gaussian, .radius = 1.0f},
        .spp         = 65536u,
        .position    = make_float3(0.0f, -6.8f, 1.0f),
        .lookat      = make_float3(0.0f, 0.0f, 1.0f),
        .up          = make_float3(0.0f, 0.0f, 1.0f),
        // pinhole
        .fov = 19.5f,
    };

    scene_info.shape_infos.resize(8);

    scene_info.shape_infos[0] =
        Shape::CreateInfo{
            .path         = "scene/cornell-box/mesh/backwall.obj",
            .surface_info = {
                .type        = Surface::Type::diffuse,
                .reflectance = {.v = make_float4(0.725f, 0.71f, 0.68f, 1.0f)}}};
    scene_info.shape_infos[1] =
        Shape::CreateInfo{
            .path         = "scene/cornell-box/mesh/ceiling.obj",
            .surface_info = {
                .type        = Surface::Type::diffuse,
                .reflectance = {.v = make_float4(0.725f, 0.71f, 0.68f, 1.0f)}}};
    scene_info.shape_infos[2] =
        Shape::CreateInfo{
            .path         = "scene/cornell-box/mesh/floor.obj",
            .surface_info = {
                .type        = Surface::Type::diffuse,
                .reflectance = {.v = make_float4(0.725f, 0.71f, 0.68f, 1.0f)}}};
    scene_info.shape_infos[3] =
        Shape::CreateInfo{
            .path         = "scene/cornell-box/mesh/leftwall.obj",
            .surface_info = {
                .type        = Surface::Type::diffuse,
                .reflectance = {.v = make_float4(0.63f, 0.065f, 0.05f, 1.0f)}}};
    scene_info.shape_infos[4] =
        Shape::CreateInfo{
            .path       = "scene/cornell-box/mesh/light.obj",
            .light_info = {
                .type     = Light::Type::diffuse,
                .emission = {.v = make_float4(17.0f, 12.0f, 4.0f, 1.0f)}}};
    scene_info.shape_infos[5] =
        Shape::CreateInfo{
            .path         = "scene/cornell-box/mesh/rightwall.obj",
            .surface_info = {
                .type        = Surface::Type::diffuse,
                .reflectance = {.v = make_float4(0.14f, 0.45f, 0.091f, 1.0f)}}};
    scene_info.shape_infos[6] =
        Shape::CreateInfo{
            .path         = "scene/cornell-box/mesh/shortbox.obj",
            .surface_info = {
                .type        = Surface::Type::diffuse,
                .reflectance = {.v = make_float4(0.725f, 0.71f, 0.68f, 1.0f)}}};
    scene_info.shape_infos[7] =
        Shape::CreateInfo{
            .path         = "scene/cornell-box/mesh/tallbox.obj",
            .surface_info = {
                .type        = Surface::Type::diffuse,
                .reflectance = {.v = make_float4(0.725f, 0.71f, 0.68f, 1.0f)}}};

    return scene_info;
}

[[nodiscard]] Scene::CreateInfo load_pbrt_scene_create_info(std::filesystem::path const& scene_path)
{
    LUISA_INFO("PBRT scene '{}' requested. PBRT loader is not implemented yet; falling back to the built-in Cornell Box scene.",
               scene_path.string());
    return make_default_cornell_box_scene_info();
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        print_usage_and_exit(argv[0]);
    }

    bool interactive     = false;
    bool has_scene_path  = false;
    std::filesystem::path scene_path{};

    for (int i = 2; i < argc; i++)
    {
        auto arg = luisa::string_view{argv[i]};
        if (is_interactive_arg(arg))
        {
            interactive = true;
            continue;
        }

        if (!arg.empty() && arg.front() == '-')
        {
            LUISA_ERROR("Unknown option '{}'. Usage: {} <backend> [scene.pbrt] [--interactive|-i].",
                        arg, argv[0]);
        }

        auto candidate = std::filesystem::path{argv[i]};
        if (!is_pbrt_scene_path(candidate))
        {
            LUISA_ERROR("Unsupported scene file '{}'. Expected a .pbrt file.",
                        candidate.string());
        }

        if (has_scene_path)
        {
            LUISA_ERROR("Multiple scene files specified: '{}' and '{}'. Only one .pbrt scene path is supported.",
                        scene_path.string(), candidate.string());
        }

        scene_path     = candidate;
        has_scene_path = true;
    }

    Application::CreateInfo app_info{
        .bin         = argv[0],
        .backend     = argv[1],
        .scene_info  = has_scene_path ? load_pbrt_scene_create_info(scene_path) : make_default_cornell_box_scene_info(),
        .interactive = interactive,
    };

    Application app{app_info};
    app.run();

    return 0;
}
