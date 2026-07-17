#include "interaction.h"

namespace Yutrel
{
Float3 Interaction::p_robust(Expr<float3> w) const noexcept
{
    // Select the offset side with the geometric normal, as PBRT does.
    auto front = dot(n_g, w) > 0.0f;
    auto n     = ite(front, n_g, -n_g);
    return p_s + n * 1e-4f;
}

Var<Ray> Interaction::spawn_ray(Expr<float3> wi, Expr<float> t_max) const noexcept
{
    return make_ray(p_robust(wi), wi, 0.0f, t_max);
}

Var<Ray> Interaction::spawn_ray_to(Expr<float3> p) const noexcept
{
    auto p_from = p_robust(p - p_s);
    auto L      = p - p_from;
    auto d      = length(L);
    auto wi     = L * (1.0f / d);
    return make_ray(p_from, wi, 0.0f, d * 0.999f);
}
} // namespace Yutrel
