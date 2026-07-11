#include "surface.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "base/interaction.h"

namespace Yutrel
{
Surface::Surface(bool two_sided) noexcept
    : m_two_sided{two_sided} {}

void Surface::Instance::closure(PolymorphicCall<Closure>& call, const Interaction& it, Expr<float3> wo, SampledWavelengths& swl, Expr<float> time) const noexcept
{
    auto cls         = call.collect(closure_identifier(), [&]
    {
        return create_closure(swl, time);
    });
    auto oriented_it = it;
    if (base()->two_sided())
    {
        auto flip           = dot(wo, it.shading.n()) < 0.0f;
        oriented_it.shading = it.shading.flipped(flip);
    }
    populate_closure(cls, oriented_it);
}

static auto validate_surface_sides(Expr<float3> ng, Expr<float3> ns,
                                   Expr<float3> wo, Expr<float3> wi) noexcept
{
    static Callable is_valid = [](Float3 ng, Float3 ns, Float3 wo, Float3 wi) noexcept
    {
        auto flip = sign(dot(ng, ns));
        return sign(flip * dot(wo, ns)) == sign(dot(wo, ng)) &
               sign(flip * dot(wi, ns)) == sign(dot(wi, ng));
    };
    return is_valid(ng, ns, wo, wi);
}

Surface::Sample Surface::Closure::sample(Expr<float3> wo, Expr<float> u_lobe, Expr<float2> u) const noexcept
{
    auto s = Surface::Sample::zero(swl().dimension());
    $outline
    {
        s          = sample_impl(wo, u_lobe, u);
        auto valid = validate_surface_sides(it().n_g, it().shading.n(), wo, s.wi);
        s.eval.f   = ite(valid, s.eval.f, 0.0f);
        s.eval.pdf = ite(valid, s.eval.pdf, 0.0f);
    };

    return s;
}

Surface::Evaluation Surface::Closure::evaluate(Expr<float3> wo, Expr<float3> wi) const noexcept
{
    auto eval = Surface::Evaluation::zero(swl().dimension());
    $outline
    {
        eval       = evaluate_impl(wo, wi);
        auto valid = validate_surface_sides(it().n_g, it().shading.n(), wo, wi);
        eval.f     = ite(valid, eval.f, 0.0f);
        eval.pdf   = ite(valid, eval.pdf, 0.0f);
    };
    return eval;
}
} // namespace Yutrel
