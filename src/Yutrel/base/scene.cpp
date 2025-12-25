#include "scene.h"

#include "base/camera.h"

namespace Yutrel
{
    Scene::Scene(const Context& context) noexcept
        : m_context(context) {}

    Scene::~Scene() noexcept = default;

    luisa::unique_ptr<Scene> Scene::create(const Context& context) noexcept
    {
        auto scene = luisa::make_unique<Scene>(context);

        // todo : create camera
        Camera::CreateInfo camera_info{
            .type                  = Camera::Type::thin_lens,
            .spp                   = 1024u,
            .shutter_span          = {0.0f, 1.0f},
            .shutter_samples_count = 64u,
            .position              = make_float3(13.0f, 2.0f, 3.0f),
            .lookat                = make_float3(0.0f, 0.0f, 0.0f),
            .up                    = make_float3(0.0f, 1.0f, 0.0f),
            // pinhole
            .fov = 20.0f,
            // thin lens
            .aperture       = 0.4f,
            .focal_length   = 78.0f,
            .focus_distance = 13.0f,
        };

        scene->m_camera = Camera::create(camera_info);

        return scene;
    }

    Camera* Scene::camera() const noexcept { return m_camera.get(); }
} // namespace Yutrel