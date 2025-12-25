#include "scene.h"

namespace Yutrel
{
    struct Scene::Config
    {
        luisa::unique_ptr<Camera> camera;
        luisa::vector<luisa::unique_ptr<Texture>> textures;
    };

    Scene::Scene(const Context& context) noexcept
        : m_context(context),
          m_config(luisa::make_unique<Config>()) {}

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

        scene->load_camera(camera_info);

        return scene;
    }

    void Scene::load_camera(const Camera::CreateInfo& info) const noexcept
    {
        if (m_config->camera)
        {
            LUISA_ERROR("Multiple cameras are not supported yet.");
            return;
        }
        m_config->camera = Camera::create(info);
    }

    const Texture* Scene::load_texture(const Texture::CreateInfo& info) const noexcept
    {
        return m_config->textures.emplace_back(Texture::create(info)).get();
    }

    const Camera* Scene::camera() const noexcept
    {
        return m_config->camera.get();
    }

} // namespace Yutrel