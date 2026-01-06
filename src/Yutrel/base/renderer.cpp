#include "renderer.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/geometry.h"
#include "base/integrator.h"
#include "base/scene.h"

namespace Yutrel
{
Renderer::Renderer(Device& device) noexcept
    : m_device(device),
      m_bindless_array(device.create_bindless_array()) {}

Renderer::~Renderer() noexcept = default;

uint Renderer::register_surface(CommandBuffer& command_buffer, const Surface* surface) noexcept
{
    if (auto iter = m_surface_tags.find(surface);
        iter != m_surface_tags.end())
    {
        return iter->second;
    }
    auto tag = m_surfaces.emplace(surface->build(*this, command_buffer));
    m_surface_tags.emplace(surface, tag);
    return tag;
}

uint Renderer::register_light(CommandBuffer& command_buffer, const Light* light) noexcept
{
    if (auto iter = m_light_tags.find(light);
        iter != m_light_tags.end())
    {
        return iter->second;
    }
    auto tag = m_lights.emplace(light->build(*this, command_buffer));
    m_light_tags.emplace(light, tag);
    return tag;
}

luisa::unique_ptr<Renderer> Renderer::create(Device& device, Stream& stream, const Scene& scene) noexcept
{
    auto renderer = luisa::make_unique<Renderer>(device);

    CommandBuffer command_buffer{stream};

    auto update_bindless_if_dirty = [&renderer, &command_buffer]
    {
        if (renderer->m_bindless_array.dirty())
        {
            command_buffer << renderer->m_bindless_array.update();
        }
    };

    renderer->m_camera = scene.camera()->build(*renderer, command_buffer);
    update_bindless_if_dirty();

    renderer->m_geometry = luisa::make_unique<Geometry>(*renderer);
    renderer->m_geometry->build(command_buffer, scene.shapes());
    update_bindless_if_dirty();

    renderer->m_integrator = Integrator::create(*renderer, command_buffer);
    update_bindless_if_dirty();

    command_buffer << synchronize();

    return renderer;
}

void Renderer::render(Stream& stream)
{
    m_integrator->render(stream);
}

void Renderer::render_interactive(Stream& stream)
{
    m_integrator->render_interactive(stream);
}

const Texture::Instance* Renderer::build_texture(CommandBuffer& command_buffer, const Texture* texture) noexcept
{
    if (texture == nullptr)
        return nullptr;
    if (auto iter = m_textures.find(texture); iter != m_textures.end())
    {
        return iter->second.get();
    }
    auto t = texture->build(*this, command_buffer);
    return m_textures.emplace(texture, std::move(t)).first->second.get();
}

} // namespace Yutrel