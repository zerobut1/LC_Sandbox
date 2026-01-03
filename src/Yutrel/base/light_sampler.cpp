#include "light_sampler.h"

#include "base/geometry.h"
#include "base/renderer.h"

namespace Yutrel
{
luisa::unique_ptr<LightSampler> LightSampler::create(Renderer& renderer, CommandBuffer& command_buffer) noexcept
{
    return luisa::make_unique<LightSampler>(renderer, command_buffer);
}

LightSampler::LightSampler(Renderer& renderer, CommandBuffer& command_buffer) noexcept
    : m_renderer(renderer)
{
    if (!renderer.lights().empty())
    {
        auto [view, buffer_id] = renderer.bindless_arena_buffer<Light::Handle>(renderer.lights().size());
        m_light_buffer_id      = buffer_id;
        command_buffer
            << view.copy_from(renderer.geometry()->light_instances().data())
            << commit();
    }
}

} // namespace Yutrel