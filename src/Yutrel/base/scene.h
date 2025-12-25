#pragma once

#include <luisa/luisa-compute.h>

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
    };

} // namespace Yutrel