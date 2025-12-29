#include "material.h"

#include "base/renderer.h"
#include "base/texture.h"
#include "utils/command_buffer.h"

namespace Yutrel::RTWeekend
{
    // Lambertian
    luisa::unique_ptr<Material::Instance> Lambertian::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
    {
        auto albedo = renderer.build_texture(command_buffer, m_albedo);
        return luisa::make_unique<Instance>(renderer, this, albedo);
    }

    // Metal
    luisa::unique_ptr<Material::Instance> Metal::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
    {
        auto albedo_fuzz = renderer.build_texture(command_buffer, m_albedo_fuzz);
        return luisa::make_unique<Instance>(renderer, this, albedo_fuzz);
    }

    // Dielectric
    luisa::unique_ptr<Material::Instance> Dielectric::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
    {
        return luisa::make_unique<Instance>(renderer, this, m_ior);
    }

    // DiffuseLight
    luisa::unique_ptr<Material::Instance> DiffuseLight::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
    {
        auto emit = renderer.build_texture(command_buffer, m_emit);
        return luisa::make_unique<Instance>(renderer, this, emit);
    }

} // namespace Yutrel::RTWeekend
