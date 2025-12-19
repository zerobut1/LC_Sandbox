#include "sampling.h"

namespace Yutrel
{
    Float2 sample_uniform_disk_concentric(Expr<float2> u) noexcept
    {
        static Callable impl = [](Float2 u_in) noexcept
        {
            auto u     = u_in * 2.0f - 1.0f;
            auto p     = abs(u.x) > abs(u.y);
            auto r     = ite(p, u.x, u.y);
            auto theta = ite(p, pi_over_four * (u.y / u.x), pi_over_two - pi_over_four * (u.x / u.y));
            return r * make_float2(cos(theta), sin(theta));
        };
        return impl(u);
    }

    Float3 sample_cosine_hemisphere(Expr<float2> u) noexcept
    {
        static Callable impl = [](Float2 u) noexcept
        {
            auto d = sample_uniform_disk_concentric(u);
            auto z = sqrt(max(1.0f - d.x * d.x - d.y * d.y, 0.0f));
            return make_float3(d.x, d.y, z);
        };
        return impl(u);
    }

    Float3 sample_uniform_sphere(Expr<float2> u) noexcept
    {
        static Callable impl = [](Float2 u) noexcept
        {
            auto z   = 1.0f - 2.0f * u.x;
            auto r   = sqrt(max(1.0f - z * z, 0.0f));
            auto phi = 2.0f * pi * u.y;
            return make_float3(r * cos(phi), r * sin(phi), z);
        };
        return impl(u);
    }
} // namespace Yutrel