#include "base/application.h"

#include <luisa/core/logging.h>

using namespace Yutrel;

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        LUISA_ERROR("Usage: {} <backend> [--interactive|-i]. <backend>: cuda, dx, vk", argv[0]);
        exit(1);
    }

    bool interactive = false;
    for (int i = 2; i < argc; i++)
    {
        auto arg = luisa::string_view{argv[i]};
        if (arg == "--interactive" || arg == "-i" || arg == "interactive")
        {
            interactive = true;
        }
    }

    Application::CreateInfo app_info{
        .bin         = argv[0],
        .backend     = argv[1],
        .interactive = interactive,
    };

    auto& scene_info = app_info.scene_info;

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

    Application app{app_info};
    app.run();

    return 0;
}