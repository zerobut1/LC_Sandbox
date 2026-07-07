#include "spectrum/hero.h"

#include "base/renderer.h"
#include "utils/color_space.h"
#include "utils/rgb2spec.h"

namespace Yutrel
{

HeroWavelengthSpectrum::Instance::Instance(Renderer& renderer, CommandBuffer& command_buffer, const Spectrum* spectrum, uint t0) noexcept
    : Spectrum::Instance(renderer, command_buffer, spectrum),
      m_illum_d65(SPD::create_cie_d65(renderer, command_buffer)),
      m_rgb2spec_t0(t0) {}

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
    auto spec = RGBAlbedoSpectrum{RGBSigmoidPolynomial{v.xyz()}};
    SampledSpectrum s{base()->dimension()};
    for (auto i = 0u; i < s.dimension(); i++)
    {
        s[i] = spec.sample(swl.lambda(i));
    }
    return {.value = s, .strength = v.w};
}

Spectrum::Decode HeroWavelengthSpectrum::Instance::decode_unbounded(
    const SampledWavelengths& swl, Expr<float4> v) const noexcept
{
    auto spec = RGBAlbedoSpectrum{RGBSigmoidPolynomial{v.xyz()}};
    SampledSpectrum s{base()->dimension()};
    for (auto i = 0u; i < s.dimension(); i++)
    {
        s[i] = spec.sample(swl.lambda(i));
    }
    return {.value = s, .strength = v.w};
}

Spectrum::Decode HeroWavelengthSpectrum::Instance::decode_illuminant(
    const SampledWavelengths& swl, Expr<float4> v) const noexcept
{
    auto spec = RGBIlluminantSpectrum{RGBSigmoidPolynomial{v.xyz()}, v.w, m_illum_d65};
    SampledSpectrum s{base()->dimension()};
    for (auto i = 0u; i < s.dimension(); i++)
    {
        s[i] = spec.sample(swl.lambda(i));
    }
    return {.value = s, .strength = v.w};
}

Float4 HeroWavelengthSpectrum::Instance::encode_srgb_albedo(Expr<float3> rgb) const noexcept
{
    return RGB2SpectrumTable::srgb().decode_albedo(renderer().bindless_array(), m_rgb2spec_t0, rgb);
}

Float4 HeroWavelengthSpectrum::Instance::encode_srgb_unbounded(Expr<float3> rgb) const noexcept
{
    return RGB2SpectrumTable::srgb().decode_unbounded(renderer().bindless_array(), m_rgb2spec_t0, rgb);
}

Float4 HeroWavelengthSpectrum::Instance::encode_srgb_illuminant(Expr<float3> rgb) const noexcept
{
    return RGB2SpectrumTable::srgb().decode_unbounded(renderer().bindless_array(), m_rgb2spec_t0, rgb);
}

luisa::unique_ptr<Spectrum::Instance> HeroWavelengthSpectrum::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto rgb2spec_t0 = renderer.create<Volume<float>>(PixelStorage::FLOAT4, make_uint3(RGB2SpectrumTable::s_resolution));
    auto rgb2spec_t1 = renderer.create<Volume<float>>(PixelStorage::FLOAT4, make_uint3(RGB2SpectrumTable::s_resolution));
    auto rgb2spec_t2 = renderer.create<Volume<float>>(PixelStorage::FLOAT4, make_uint3(RGB2SpectrumTable::s_resolution));
    RGB2SpectrumTable::srgb().encode(command_buffer, *rgb2spec_t0, *rgb2spec_t1, *rgb2spec_t2);
    auto t0 = renderer.register_bindless(*rgb2spec_t0, TextureSampler::linear_point_zero());
    auto t1 = renderer.register_bindless(*rgb2spec_t1, TextureSampler::linear_point_zero());
    auto t2 = renderer.register_bindless(*rgb2spec_t2, TextureSampler::linear_point_zero());
    LUISA_ASSERT(
        t1 == t0 + 1u && t2 == t0 + 2u,
        "Invalid RGB2Spec texture indices: "
        "{}, {}, and {}.",
        t0,
        t1,
        t2);
    return luisa::make_unique<Instance>(renderer, command_buffer, this, t0);
}

float4 HeroWavelengthSpectrum::encode_static_srgb_albedo(float3 rgb) const noexcept
{
    return RGB2SpectrumTable::srgb().decode_albedo(rgb);
}

float4 HeroWavelengthSpectrum::encode_static_srgb_unbounded(float3 rgb) const noexcept
{
    return RGB2SpectrumTable::srgb().decode_unbounded(rgb);
}

float4 HeroWavelengthSpectrum::encode_static_srgb_illuminant(float3 rgb) const noexcept
{
    return RGB2SpectrumTable::srgb().decode_unbounded(rgb);
}
} // namespace Yutrel
