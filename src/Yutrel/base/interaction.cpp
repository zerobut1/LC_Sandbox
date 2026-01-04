#include "interaction.h"

namespace Yutrel
{
Var<Ray> Interaction::spawn_ray(Expr<float3> wi, Expr<float> t_max) const noexcept
{
    auto n      = n_g;
    auto offset = n * 1e-4f;
    auto origin = p_s + ite(dot(wi, n) > 0.0f, offset, -offset);
    return make_ray(origin, wi, 0.0f, t_max);
}

Var<Ray> Interaction::spawn_ray_to(Expr<float3> p) const noexcept
{
    auto p_from = p_s;
    auto L      = p - p_from;
    auto d      = length(L);
    auto wi     = L * (1.0f / d);
    auto n      = n_g;
    auto offset = n * 1e-4f;
    auto origin = p_from + ite(dot(wi, n) > 0.0f, offset, -offset);
    return make_ray(origin, wi, 0.0f, d * 0.999f);
}
} // namespace Yutrel