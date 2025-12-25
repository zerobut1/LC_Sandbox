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

    const Texture* Scene::load_texture(const Texture::CreateInfo& info) noexcept
    {
        return m_config->textures.emplace_back(Texture::create(*this, info)).get();
    }

    const Camera* Scene::camera() const noexcept
    {
        return m_config->camera.get();
    }

} // namespace Yutrel