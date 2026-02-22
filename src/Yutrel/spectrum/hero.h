#pragma once

#include "base/spectrum.h"

namespace Yutrel
{
class HeroWavelengthSpectrum : public Spectrum
{
private:
    static constexpr uint s_dimension = 4u;

public:
    class Instance : public Spectrum::Instance
    {
    private:
        [[nodiscard]] static Float gaussian(Expr<float> lambda, float mean, float sigma) noexcept;
        [[nodiscard]] static SampledSpectrum decode_from_srgb(
            const SampledWavelengths& swl, Expr<float3> rgb) noexcept;

    public:
        explicit Instance(Renderer& renderer, CommandBuffer& command_buffer, const Spectrum* spectrum) noexcept;

        [[nodiscard]] SampledWavelengths sample(Expr<float> u) const noexcept override;
        [[nodiscard]] Spectrum::Decode decode_albedo(const SampledWavelengths& swl, Expr<float4> v) const noexcept override;
        [[nodiscard]] Spectrum::Decode decode_unbounded(const SampledWavelengths& swl, Expr<float4> v) const noexcept override;
        [[nodiscard]] Spectrum::Decode decode_illuminant(const SampledWavelengths& swl, Expr<float4> v) const noexcept override;
        [[nodiscard]] Float cie_y(const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept override;
        [[nodiscard]] Float3 cie_xyz(const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept override;
        [[nodiscard]] Float3 srgb(const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept override;
        [[nodiscard]] Float4 encode_srgb_albedo(Expr<float3> rgb) const noexcept override;
        [[nodiscard]] Float4 encode_srgb_unbounded(Expr<float3> rgb) const noexcept override;
        [[nodiscard]] Float4 encode_srgb_illuminant(Expr<float3> rgb) const noexcept override;
    };

public:
    HeroWavelengthSpectrum(Scene& scene, const CreateInfo& info) noexcept
        : Spectrum(scene, info) {}

    [[nodiscard]] luisa::unique_ptr<Spectrum::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;

    [[nodiscard]] bool is_fixed() const noexcept override { return false; }
    [[nodiscard]] uint dimension() const noexcept override { return s_dimension; }
    [[nodiscard]] float4 encode_static_srgb_albedo(float3 rgb) const noexcept override;
    [[nodiscard]] float4 encode_static_srgb_unbounded(float3 rgb) const noexcept override;
    [[nodiscard]] float4 encode_static_srgb_illuminant(float3 rgb) const noexcept override;
};
} // namespace Yutrel
