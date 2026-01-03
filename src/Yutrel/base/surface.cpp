#include "surface.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "surfaces/diffuse.h"
#include "surfaces/null.h"

namespace Yutrel
{
luisa::unique_ptr<Surface> Surface::create(Scene& scene, const CreateInfo& info) noexcept
{
    switch (info.type)
    {
    case Type::diffuse:
        return luisa::make_unique<Diffuse>(scene, info);
    case Type::null:
    default:
        return luisa::make_unique<NullSurface>(scene, info);
    }
}

void Surface::Instance::closure(PolymorphicCall<Closure>& call, const Interaction& it, Expr<float> time) const noexcept
{
    auto cls = call.collect(closure_identifier(), [&]
    {
        return create_closure(time);
    });
    populate_closure(cls, it);
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
    auto s = Surface::Sample::zero();
    $outline
    {
        s          = sample_impl(wo, u_lobe, u);
        auto valid = validate_surface_sides(it().n_g, it().shading.n(), wo, s.wi);
        s.eval.f   = ite(valid, s.eval.f, make_float3(0.0f));
        s.eval.pdf = ite(valid, s.eval.pdf, 0.0f);
    };

    return s;
}
} // namespace Yutrel