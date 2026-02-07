#include "spectrum.h"

#include "base/scene.h"
#include "spectrum/srgb.h"

namespace Yutrel
{
luisa::unique_ptr<Spectrum> Spectrum::create(Scene& scene, const CreateInfo& info) noexcept
{
    return luisa::make_unique<SRGBSpectrum>(scene, info);
}

Spectrum::Spectrum(Scene& scene, const CreateInfo& info) noexcept
{
}

Spectrum::Instance::Instance(const Renderer& renderer, CommandBuffer& command_buffer, const Spectrum* spectrum) noexcept
    : m_renderer(renderer), m_spectrum(spectrum)
{
}

Float Spectrum::Instance::cie_y(const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept
{
    return 0.0f;
}

Float3 Spectrum::Instance::cie_xyz(const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept
{
    return make_float3(0.0f);
}

Float3 Spectrum::Instance::srgb(const SampledWavelengths& swl, const SampledSpectrum& sp) const noexcept
{
    return make_float3(0.0f);
}

} // namespace Yutrel