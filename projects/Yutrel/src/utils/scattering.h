#pragma once

#include <luisa/dsl/syntax.h>

#include "frame.h"
#include "spectra.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

enum TransportMode
{
    RADIANCE,
    IMPORTANCE
};

class BxDF
{
public:
    struct SampledDirection
    {
        Float3 wi;
        Bool valid;
    };

public:
    virtual ~BxDF() noexcept = default;

    [[nodiscard]] virtual SampledSpectrum evaluate(Expr<float3> wo, Expr<float3> wi, TransportMode mode) const noexcept = 0;
    [[nodiscard]] virtual SampledDirection sample_wi(Expr<float3> wo, Expr<float2> u, TransportMode mode) const noexcept;
    [[nodiscard]] virtual SampledSpectrum sample(Expr<float3> wo, Float3* wi, Expr<float2> u, Float* pdf, TransportMode mode) const noexcept;
    [[nodiscard]] virtual Float pdf(Expr<float3> wo, Expr<float3> wi, TransportMode mode) const noexcept;
    [[nodiscard]] virtual SampledSpectrum albedo() const noexcept = 0;
};

class LambertianReflection final : public BxDF
{
private:
    SampledSpectrum m_reflectance;

public:
    explicit LambertianReflection(const SampledSpectrum& reflectance) noexcept
        : m_reflectance{reflectance} {}

    [[nodiscard]] SampledSpectrum evaluate(Expr<float3> wo, Expr<float3> wi, TransportMode mode) const noexcept override;
    [[nodiscard]] SampledSpectrum albedo() const noexcept override { return m_reflectance; }
};

[[nodiscard]] Bool refract(Expr<float3> wi, Expr<float3> n, Expr<float> eta, Float3* wt) noexcept;

} // namespace Yutrel

LUISA_DISABLE_DSL_ADDRESS_OF_OPERATOR(Yutrel::BxDF)
LUISA_DISABLE_DSL_ADDRESS_OF_OPERATOR(Yutrel::BxDF::SampledDirection)
LUISA_DISABLE_DSL_ADDRESS_OF_OPERATOR(Yutrel::LambertianReflection)
