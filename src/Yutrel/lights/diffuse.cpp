#include "diffuse.h"

#include "base/interaction.h"
#include "base/renderer.h"
#include "base/scene.h"

namespace Yutrel
{
DiffuseLight::DiffuseLight(Scene& scene, const CreateInfo& info) noexcept
    : Light(scene, info),
      m_emission(scene.load_texture(info.emission)),
      m_scale(info.scale),
      m_two_sided(info.two_sided) {}

luisa::unique_ptr<Light::Instance> DiffuseLight::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto emission = renderer.build_texture(command_buffer, m_emission);

    return luisa::make_unique<Instance>(renderer, this, emission);
}

luisa::unique_ptr<Light::Closure> DiffuseLight::Instance::closure(Expr<float> time) const noexcept
{
    return luisa::make_unique<Closure>(this, time);
}

Light::Evaluation DiffuseLight::Closure::evaluate(const Interaction& it_light, Expr<float3> p_from) const noexcept
{
    auto eval = Light::Evaluation::zero();

    $outline
    {
        auto light      = instance<DiffuseLight::Instance>();
        auto&& renderer = light->renderer();

        auto pdf_triangle = renderer.buffer<float>(it_light.shape.pdf_buffer_id()).read(it_light.prim_id);
        auto pdf_area     = pdf_triangle / it_light.prim_area;
        auto cos_wo       = abs(dot(normalize(p_from - it_light.p_g), it_light.n_g));
        auto L            = light->texture()->evaluate(it_light).xyz();
        auto pdf          = distance_squared(it_light.p_g, p_from) * pdf_area * (1.0f / cos_wo);
        auto two_sided    = light->base<DiffuseLight>()->two_sided();
        auto invalid      = abs(cos_wo) < 1e-6f | (!two_sided & !it_light.front_face);
        eval              = {.L   = ite(invalid, make_float3(0.0f), L),
                             .pdf = ite(invalid, 0.0f, pdf),
                             .p   = it_light.p_g,
                             .ng  = it_light.shading.n()};
    };
    return eval;
}

} // namespace Yutrel