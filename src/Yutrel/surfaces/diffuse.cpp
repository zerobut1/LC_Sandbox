#include "diffuse.h"

#include "base/renderer.h"
#include "base/scene.h"
#include "utils/sampling.h"

namespace Yutrel
{
Diffuse::Diffuse(Scene& scene, const CreateInfo& info) noexcept
    : Surface(scene, info), m_reflectance(scene.load_texture(info.reflectance)) {}

luisa::unique_ptr<Surface::Instance> Diffuse::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto reflectance = renderer.build_texture(command_buffer, m_reflectance);

    return luisa::make_unique<Instance>(renderer, this, reflectance);
}

luisa::unique_ptr<Surface::Closure> Diffuse::Instance::create_closure(Expr<float> time) const noexcept
{
    return luisa::make_unique<Closure>(renderer(), time);
}

void Diffuse::Instance::populate_closure(Surface::Closure* closure, const Interaction& it) const noexcept
{
    Float3 reflectance = m_reflectance->evaluate(it).xyz();

    Diffuse::Closure::Context ctx{
        .it          = it,
        .reflectance = reflectance,
    };
    closure->bind(std::move(ctx));
}

Surface::Sample Diffuse::Closure::sample_impl(Expr<float3> wo, Expr<float> u_lobe, Expr<float2> u) const noexcept
{
    auto&& ctx = context<Context>();

    auto wi_local = sample_cosine_hemisphere(u);
    auto wi       = ctx.it.shading.local_to_world(wi_local);
    auto f        = ctx.reflectance * inv_pi;
    auto pdf      = abs_cos_theta(wi_local) * inv_pi;

    return Surface::Sample{
        .eval = {
            .f           = f * abs_cos_theta(wi_local),
            .pdf         = pdf,
            .f_diffuse   = f * abs_cos_theta(wi_local),
            .pdf_diffuse = pdf,
        },
        .wi    = wi,
        .event = Surface::event_reflect};
}

Surface::Evaluation Diffuse::Closure::evaluate_impl(Expr<float3> wo, Expr<float3> wi) const noexcept
{
    auto&& ctx = context<Context>();

    auto wi_local = ctx.it.shading.world_to_local(wi);
    auto f        = ctx.reflectance * inv_pi;
    auto pdf      = abs_cos_theta(wi_local) * inv_pi;

    return Surface::Evaluation{
        .f           = f * abs_cos_theta(wi_local),
        .pdf         = pdf,
        .f_diffuse   = f * abs_cos_theta(wi_local),
        .pdf_diffuse = pdf,
    };
}

} // namespace Yutrel