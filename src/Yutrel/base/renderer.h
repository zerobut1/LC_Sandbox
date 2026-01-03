#pragma once

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/light.h"
#include "base/surface.h"
#include "base/texture.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;
using TextureSampler = compute::Sampler;

class Scene;
class Integrator;
class Geometry;

class Renderer final
{
private:
    Device& m_device;
    luisa::vector<luisa::unique_ptr<Resource>> m_resources;
    BindlessArray m_bindless_array;
    size_t m_bindless_buffer_count{0u};
    size_t m_bindless_tex2d_count{0u};
    Polymorphic<Surface::Instance> m_surfaces;
    Polymorphic<Light::Instance> m_lights;
    luisa::unordered_map<const Surface*, uint> m_surface_tags;
    luisa::unordered_map<const Light*, uint> m_light_tags;
    luisa::unordered_map<const Texture*, luisa::unique_ptr<Texture::Instance>> m_textures;

    luisa::unique_ptr<Camera::Instance> m_camera;
    luisa::unique_ptr<Integrator> m_integrator;
    luisa::unique_ptr<Geometry> m_geometry;

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

    template <typename T>
    [[nodiscard]] std::pair<BufferView<T>, uint /* bindless id */> bindless_arena_buffer(size_t n) noexcept
    {
        auto view      = arena_buffer<T>(n);
        auto buffer_id = register_bindless(view);
        return std::make_pair(view, buffer_id);
    }

    template <typename T>
    [[nodiscard]] auto register_bindless(BufferView<T> buffer) noexcept
    {
        auto buffer_id = m_bindless_buffer_count++;
        m_bindless_array.emplace_on_update(buffer_id, buffer);
        return static_cast<uint>(buffer_id);
    }

    template <typename T>
    [[nodiscard]] auto register_bindless(const Buffer<T>& buffer) noexcept
    {
        return register_bindless(buffer.view());
    }

    template <typename T>
    [[nodiscard]] auto register_bindless(const Image<T>& image, TextureSampler sampler) noexcept
    {
        auto tex2d_id = m_bindless_tex2d_count++;
        m_bindless_array.emplace_on_update(tex2d_id, image, sampler);
        return static_cast<uint>(tex2d_id);
    }

    [[nodiscard]] uint register_surface(CommandBuffer& command_buffer, const Surface* surface) noexcept;
    [[nodiscard]] uint register_light(CommandBuffer& command_buffer, const Light* light) noexcept;

public:
    [[nodiscard]] static luisa::unique_ptr<Renderer> create(Device& device, Stream& stream, const Scene& scene) noexcept;

    void render(Stream& stream);

    [[nodiscard]] auto& device() const noexcept { return m_device; }
    [[nodiscard]] auto camera() const noexcept { return m_camera.get(); }
    [[nodiscard]] auto integrator() const noexcept { return m_integrator.get(); }
    [[nodiscard]] auto geometry() const noexcept { return m_geometry.get(); }
    [[nodiscard]] auto& surfaces() const noexcept { return m_surfaces; }
    [[nodiscard]] auto& lights() const noexcept { return m_lights; }

    [[nodiscard]] const Texture::Instance* build_texture(CommandBuffer& command_buffer, const Texture* texture) noexcept;

    template <typename T, typename I>
    [[nodiscard]] auto buffer(I&& id) const noexcept { return m_bindless_array->buffer<T>(std::forward<I>(id)); }
    template <typename T>
    [[nodiscard]] auto tex2d(T&& id) const noexcept { return m_bindless_array->tex2d(std::forward<T>(id)); }
};

} // namespace Yutrel