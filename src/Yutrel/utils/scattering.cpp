#include "scattering.h "

namespace Yutrel
{
[[nodiscard]] Bool refract(Expr<float3> wi, Expr<float3> n, Expr<float> eta, Float3* wt) noexcept
{
    static Callable impl = [](Float3 wi, Float3 n, Float eta) noexcept
    {
        // Compute $\cos \theta_\roman{t}$ using Snell's law
        auto cosThetaI  = dot(n, wi);
        auto sin2ThetaI = max(0.0f, one_minus_sqr(cosThetaI));
        auto sin2ThetaT = sqr(eta) * sin2ThetaI;
        auto cosThetaT  = sqrt(1.f - sin2ThetaT);
        // Handle total internal reflection for transmission
        auto wt = (eta * cosThetaI - cosThetaT) * n - eta * wi;
        return make_float4(wt, sin2ThetaT);
    };
    auto v = impl(wi, n, eta);
    *wt    = v.xyz();
    return v.w < 1.0f;
}

} // namespace Yutrel