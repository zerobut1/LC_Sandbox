#include "diffuse.h"

#include "base/renderer.h"

namespace Yutrel
{
DiffuseLight::DiffuseLight(Scene& scene, const CreateInfo& info) noexcept
    : Light(scene, info)
{
}

luisa::unique_ptr<Light::Instance> DiffuseLight::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto emission = renderer.build_texture(command_buffer, m_emission);

    return luisa::make_unique<Instance>(renderer, this, emission);
}

luisa::unique_ptr<Light::Closure> DiffuseLight::Instance::closure(Expr<float> time) const noexcept
{
    return luisa::make_unique<Closure>(this, time);
}

} // namespace Yutrel