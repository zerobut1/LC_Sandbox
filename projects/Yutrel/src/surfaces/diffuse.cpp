#include "diffuse.h"

#include "base/renderer.h"
#include "scene/scene_builder.h"
#include "utils/scattering.h"

namespace Yutrel
{
Diffuse::Diffuse(const Texture* reflectance, bool two_sided) noexcept
    : Surface{two_sided}, m_reflectance{reflectance} {}

luisa::unique_ptr<Surface::Instance> Diffuse::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto reflectance = renderer.build_texture(command_buffer, m_reflectance);

    return luisa::make_unique<Instance>(renderer, this, reflectance);
}

luisa::unique_ptr<Surface::Closure> Diffuse::Instance::create_closure(SampledWavelengths& swl, Expr<float> time) const noexcept
{
    return luisa::make_unique<Closure>(renderer(), swl, time);
}

void Diffuse::Instance::populate_closure(Surface::Closure* closure, const Interaction& it) const noexcept
{
    auto& swl        = closure->swl();
    auto time        = closure->time();
    auto reflectance = m_reflectance->evaluate_albedo_spectrum(it, swl, time).value;

    Diffuse::Closure::Context ctx{
        .it          = it,
        .reflectance = reflectance,
    };
    closure->bind(std::move(ctx));
}

void Diffuse::Closure::pre_eval() noexcept
{
    m_bxdf = luisa::make_unique<LambertianReflection>(context<Context>().reflectance);
}

void Diffuse::Closure::post_eval() noexcept
{
    m_bxdf = nullptr;
}

Surface::Sample Diffuse::Closure::sample_impl(Expr<float3> wo, Expr<float> u_lobe, Expr<float2> u) const noexcept
{
    auto&& ctx = context<Context>();

    auto wo_local = ctx.it.shading.world_to_local(wo);
    auto wi_local = def(make_float3(0.0f, 0.0f, 1.0f));
    auto pdf      = def(0.0f);
    auto f        = m_bxdf->sample(wo_local, std::addressof(wi_local), u, std::addressof(pdf), TransportMode::RADIANCE);
    auto wi       = ctx.it.shading.local_to_world(wi_local);
    auto f_cos    = f * abs_cos_theta(wi_local);

    return Surface::Sample{
        .eval = {
            .f           = f_cos,
            .pdf         = pdf,
            .f_diffuse   = f_cos,
            .pdf_diffuse = pdf,
        },
        .wi    = wi,
        .event = Surface::event_reflect};
}

Surface::Evaluation Diffuse::Closure::evaluate_impl(Expr<float3> wo, Expr<float3> wi) const noexcept
{
    auto&& ctx = context<Context>();

    auto wo_local = ctx.it.shading.world_to_local(wo);
    auto wi_local = ctx.it.shading.world_to_local(wi);
    auto f        = m_bxdf->evaluate(wo_local, wi_local, TransportMode::RADIANCE);
    auto pdf      = m_bxdf->pdf(wo_local, wi_local, TransportMode::RADIANCE);
    auto f_cos    = f * abs_cos_theta(wi_local);

    return Surface::Evaluation{
        .f           = f_cos,
        .pdf         = pdf,
        .f_diffuse   = f_cos,
        .pdf_diffuse = pdf,
    };
}

const Surface* DiffuseSurfaceSpec::build(SceneBuilder& builder) const noexcept
{
    return builder.emplace<Surface, Diffuse>(builder.resolve(_reflectance), _two_sided);
}

} // namespace Yutrel
