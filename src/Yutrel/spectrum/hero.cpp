#include "spectrum/hero.h"

#include "utils/color_space.h"

namespace Yutrel
{
Float HeroWavelengthSpectrum::Instance::gaussian(Expr<float> lambda, float mean, float sigma) noexcept
{
    auto x = (lambda - mean) / sigma;
    return exp(-0.5f * x * x);
}

SampledSpectrum HeroWavelengthSpectrum::Instance::decode_from_srgb(const SampledWavelengths& swl, Expr<float3> rgb) noexcept
{
    auto c       = max(rgb, 0.f);
    auto w       = min(c.x, min(c.y, c.z));
    auto chroma  = max(c - make_float3(w), 0.f);
    auto samples = SampledSpectrum{swl.dimension()};
    for (auto i = 0u; i < swl.dimension(); i++)
    {
        auto lambda = swl.lambda(i);
        auto r      = gaussian(lambda, 610.0f, 35.0f);
        auto g      = gaussian(lambda, 550.0f, 30.0f);
        auto b      = gaussian(lambda, 460.0f, 25.0f);
        samples[i]  = w + chroma.x * r + chroma.y * g + chroma.z * b;
    }
    return samples;
}

HeroWavelengthSpectrum::Instance::Instance(Renderer& renderer, CommandBuffer& command_buffer, const Spectrum* spectrum) noexcept
    : Spectrum::Instance(renderer, command_buffer, spectrum) {}

SampledWavelengths HeroWavelengthSpectrum::Instance::sample(Expr<float> u) const noexcept
{
    // 均匀采样
    // TODO : 人眼响应importance sampling
    SampledWavelengths swl{base()->dimension()};

    auto lambda_span   = visible_wavelength_max - visible_wavelength_min;
    auto lambda_stride = lambda_span / static_cast<float>(base()->dimension());
    auto pdf           = 1.0f / lambda_span;
    auto lambda_hero   = visible_wavelength_min + u * lambda_span;
    for (auto i = 0u; i < swl.dimension(); i++)
    {
        auto lambda = lambda_hero + lambda_stride * static_cast<float>(i);
        lambda      = ite(lambda > visible_wavelength_max, lambda - lambda_span, lambda);
        swl.set_lambda(i, lambda);
        swl.set_pdf(i, pdf);
    }
    return swl;
}

Spectrum::Decode HeroWavelengthSpectrum::Instance::decode_albedo(
    const SampledWavelengths& swl, Expr<float4> v) const noexcept
{
    auto rgb = saturate(v.xyz());
    auto sp  = clamp(decode_from_srgb(swl, rgb), 0.0f, 1.0f);
    return {.value = sp, .strength = linear_srgb_to_cie_y(rgb)};
}

Spectrum::Decode HeroWavelengthSpectrum::Instance::decode_unbounded(
    const SampledWavelengths& swl, Expr<float4> v) const noexcept
{
    auto rgb = max(v.xyz(), 0.f);
    auto sp  = decode_from_srgb(swl, rgb);
    return {.value = sp, .strength = linear_srgb_to_cie_y(rgb)};
}

Spectrum::Decode HeroWavelengthSpectrum::Instance::decode_illuminant(
    const SampledWavelengths& swl, Expr<float4> v) const noexcept
{
    auto rgb = max(v.xyz(), 0.f);
    auto sp  = decode_from_srgb(swl, rgb);
    return {.value = sp, .strength = linear_srgb_to_cie_y(rgb)};
}

Float HeroWavelengthSpectrum::Instance::cie_y(
    const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept
{
    return linear_srgb_to_cie_y(srgb(swl, sp));
}

Float3 HeroWavelengthSpectrum::Instance::cie_xyz(
    const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept
{
    return linear_srgb_to_cie_xyz(srgb(swl, sp));
}

Float3 HeroWavelengthSpectrum::Instance::srgb(
    const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept
{
    auto rgb_sum = def(make_float3(0.0f));
    auto w_sum   = def(0.0f);
    for (auto i = 0u; i < swl.dimension(); i++)
    {
        auto lambda = swl.lambda(i);
        auto pdf    = max(swl.pdf(i), 1e-6f);
        auto w      = 1.0f / pdf;
        auto r      = gaussian(lambda, 610.0f, 35.0f);
        auto g      = gaussian(lambda, 550.0f, 30.0f);
        auto b      = gaussian(lambda, 460.0f, 25.0f);
        rgb_sum += sp[i] * make_float3(r, g, b) * w;
        w_sum += w;
    }
    return rgb_sum / max(w_sum, 1e-6f);
}

Float4 HeroWavelengthSpectrum::Instance::encode_srgb_albedo(Expr<float3> rgb) const noexcept
{
    return make_float4(clamp(rgb, 0.f, 1.f), 1.f);
}

Float4 HeroWavelengthSpectrum::Instance::encode_srgb_unbounded(Expr<float3> rgb) const noexcept
{
    return make_float4(rgb, 1.f);
}

Float4 HeroWavelengthSpectrum::Instance::encode_srgb_illuminant(Expr<float3> rgb) const noexcept
{
    return make_float4(max(rgb, 0.f), 1.f);
}

luisa::unique_ptr<Spectrum::Instance> HeroWavelengthSpectrum::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    return luisa::make_unique<Instance>(renderer, command_buffer, this);
}

float4 HeroWavelengthSpectrum::encode_static_srgb_albedo(float3 rgb) const noexcept
{
    return make_float4(clamp(rgb, 0.f, 1.f), 1.f);
}

float4 HeroWavelengthSpectrum::encode_static_srgb_unbounded(float3 rgb) const noexcept
{
    return make_float4(rgb, 1.f);
}

float4 HeroWavelengthSpectrum::encode_static_srgb_illuminant(float3 rgb) const noexcept
{
    return make_float4(max(rgb, 0.f), 1.f);
}
} // namespace Yutrel
