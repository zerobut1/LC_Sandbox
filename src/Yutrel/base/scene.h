#pragma once

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/texture.h"

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Scene
    {
    public:
        struct Config;

    private:
        const Context& m_context;
        luisa::unique_ptr<Config> m_config;

    public:
        explicit Scene(const Context& context) noexcept;
        ~Scene() noexcept;

        Scene() noexcept               = delete;
        Scene(const Scene&)            = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&)                 = delete;
        Scene& operator=(Scene&&)      = delete;

    public:
        [[nodiscard]] static luisa::unique_ptr<Scene> create(const Context& context) noexcept;

        void load_camera(const Camera::CreateInfo& info) const noexcept;
        [[nodiscard]] const Texture* load_texture(const Texture::CreateInfo& info) noexcept;

        [[nodiscard]] const Camera* camera() const noexcept;
    };

} // namespace Yutrel