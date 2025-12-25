#pragma once

#include <luisa/luisa-compute.h>

#include "base/camera.h"

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Scene;
    class Integrator;

    class Renderer final
    {
    private:
        Device& m_device;
        luisa::vector<luisa::unique_ptr<Resource>> m_resources;

        luisa::unique_ptr<Camera::Instance> m_camera;
        luisa::unique_ptr<Integrator> m_integrator;

    public:
        explicit Renderer(Device& device) noexcept;
        ~Renderer() noexcept;

        Renderer() noexcept                  = delete;
        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&)                 = delete;
        Renderer& operator=(Renderer&&)      = delete;

    public:
        template <typename T, typename... Args>
            requires std::is_base_of_v<Resource, T>
        [[nodiscard]] auto create(Args&&... args) noexcept -> T*
        {
            auto resource = luisa::make_unique<T>(m_device.create<T>(std::forward<Args>(args)...));
            auto p        = resource.get();
            m_resources.emplace_back(std::move(resource));
            return p;
        }

        template <typename T>
        [[nodiscard]] BufferView<T> arena_buffer(size_t n) noexcept
        {
            return create<Buffer<T>>(n)->view();
        }

    public:
        [[nodiscard]] static luisa::unique_ptr<Renderer> create(Device& device, Stream& stream, const Scene& scene) noexcept;
        [[nodiscard]] auto& device() const noexcept { return m_device; }
        [[nodiscard]] auto camera() const noexcept { return m_camera.get(); }
        [[nodiscard]] auto integrator() const noexcept { return m_integrator.get(); }

        void render(Stream& stream);
    };

} // namespace Yutrel