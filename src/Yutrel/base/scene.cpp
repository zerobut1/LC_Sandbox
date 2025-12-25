#include "scene.h"

namespace Yutrel
{
    struct Scene::Config
    {
    };

    Scene::~Scene() noexcept = default;

    Scene::Scene(const Context& context) noexcept
        : m_context(context),
          m_config{luisa::make_unique<Config>()} {}

    luisa::unique_ptr<Scene> Scene::create(const Context& context) noexcept
    {
        auto scene = luisa::make_unique<Scene>(context);

        return scene;
    }
} // namespace Yutrel