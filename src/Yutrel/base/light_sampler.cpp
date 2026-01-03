#include "light_sampler.h"

#include "base/geometry.h"
#include "base/interaction.h"
#include "base/light.h"
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

LightSampler::Evaluation LightSampler::evaluate_hit(const Interaction& it, Expr<float3> p_from, Expr<float> time) const noexcept
{
    auto eval = Light::Evaluation::zero();

    if (renderer().lights().empty()) [[unlikely]]
    {
        LUISA_WARNING_WITH_LOCATION("No lights in scene.");
        return eval;
    };
    renderer().lights().dispatch(it.shape.light_tag(), [&](auto light) noexcept
    {
        auto closure = light->closure(time);
        eval         = closure->evaluate(it, p_from);
    });
    auto n = static_cast<float>(renderer().lights().size());
    eval.pdf *= 1.0f / n;
    return eval;
}

} // namespace Yutrel