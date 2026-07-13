#include "scattering.h"

#include "sampling.h"

namespace Yutrel
{
SampledSpectrum BxDF::sample(Expr<float3> wo, Float3* wi, Expr<float2> u, Float* pdf, TransportMode mode) const noexcept
{
    auto wi_sample = sample_wi(wo, u, mode);
    auto valid     = wi_sample.valid;
    *wi            = wi_sample.wi;
    *pdf           = ite(valid, this->pdf(wo, *wi, mode), 0.0f);
    return ite(valid, evaluate(wo, *wi, mode), 0.0f);
}

Float BxDF::pdf(Expr<float3> wo, Expr<float3> wi, TransportMode mode) const noexcept
{
    return ite(same_hemisphere(wo, wi), abs_cos_theta(wi) * inv_pi, 0.0f);
}

BxDF::SampledDirection BxDF::sample_wi(Expr<float3> wo, Expr<float2> u, TransportMode mode) const noexcept
{
    auto wi = sample_cosine_hemisphere(u);
    wi.z *= sign(cos_theta(wo));
    return {.wi = wi, .valid = true};
}

SampledSpectrum LambertianReflection::evaluate(Expr<float3> wo, Expr<float3> wi, TransportMode mode) const noexcept
{
    return m_reflectance * ite(same_hemisphere(wo, wi), inv_pi, 0.0f);
}

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
