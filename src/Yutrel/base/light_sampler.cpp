#include "light_sampler.h"

#include "base/geometry.h"
#include "base/interaction.h"
#include "base/light.h"
#include "base/renderer.h"
#include "utils/sampling.h"

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

LightSampler::Sample LightSampler::sample(const Interaction& it_from, Expr<float> u_select, Expr<float2> u_light, Expr<float> time) const noexcept
{
    if (renderer().lights().empty())
    {
        return Sample::zero();
    }

    Selection sel = select(it_from, u_select, time);
    return sample_selection(it_from, sel, u_light, time);
}

LightSampler::Selection LightSampler::select(const Interaction& it_from, Expr<float> u, Expr<float> time) const noexcept
{
    LUISA_ASSERT(!renderer().lights().empty(), "No lights in scene.");
    auto n = static_cast<float>(renderer().lights().size());

    return {.tag = cast<uint>(clamp(u * n, 0.0f, n - 1.0f)), .prob = 1.0f / n};
}

LightSampler::Sample LightSampler::sample_selection(const Interaction& it_from, const Selection& sel, Expr<float2> u, Expr<float> time) const noexcept
{
    auto sample = Sample::zero();
    if (!renderer().lights().empty())
    {
        sample = sample_light(it_from, sel, u, time);
    }
    return sample;
}

auto LightSampler::sample_area(Expr<float3> p_from, Expr<uint> tag, Expr<float2> u_in) const noexcept
{
    auto handle                = renderer().buffer<Light::Handle>(m_light_buffer_id).read(tag);
    auto light_inst            = renderer().geometry()->instance(handle.instance_id);
    auto light_to_world        = renderer().geometry()->instance_to_world(handle.instance_id);
    auto alias_table_buffer_id = light_inst.alias_table_buffer_id();
    auto [triangle_id, ux]     = sample_alias_table(renderer().buffer<AliasEntry>(alias_table_buffer_id), light_inst.triangle_count(), u_in.x);
    auto triangle              = renderer().geometry()->triangle(light_inst, triangle_id);
    auto uv                    = sample_uniform_triangle(make_float2(ux, u_in.y)).xy();
    auto attrib                = renderer().geometry()->shading_point(light_inst, triangle, uv, light_to_world);

    return luisa::make_shared<Interaction>(Interaction{
        .shape      = std::move(light_inst),
        .p_g        = attrib.pg,
        .n_g        = attrib.ng,
        .uv         = attrib.uv,
        .p_s        = attrib.pg,
        .shading    = Frame::make(attrib.ns, attrib.dpdu),
        .inst_id    = handle.instance_id,
        .prim_id    = triangle_id,
        .prim_area  = attrib.area,
        // Match hit-case convention: front_face is true when the outgoing direction
        // (from light point to shading point) lies in the +ng hemisphere.
        .front_face = dot(attrib.ng, p_from - attrib.pg) > 0.0f,
    });
}

LightSampler::Sample LightSampler::sample_light(const Interaction& it_from, const Selection& sel, Expr<float2> u, Expr<float> time) const noexcept
{
    LUISA_ASSERT(!renderer().lights().empty(), "No lights in the scene.");
    auto it   = sample_area(it_from.p_g, sel.tag, u);
    auto eval = Light::Evaluation::zero();
    renderer().lights().dispatch(it->shape.light_tag(), [&](auto light) noexcept
    {
        auto closure = light->closure(time);
        eval         = closure->evaluate(*it, it_from.p_s);
    });
    Light::Sample light_sample = {.eval = std::move(eval), .p = it->p_g};
    light_sample.eval.pdf *= sel.prob;

    return Sample::from_light(light_sample, it_from);
}

LightSampler::Sample LightSampler::Sample::zero() noexcept
{
    return Sample{.eval = Evaluation::zero(), .shadow_ray = {}};
}

LightSampler::Sample LightSampler::Sample::from_light(const Light::Sample& s, const Interaction& it_from) noexcept
{
    return Sample{.eval = s.eval, .shadow_ray = it_from.spawn_ray_to(s.p)};
}
} // namespace Yutrel