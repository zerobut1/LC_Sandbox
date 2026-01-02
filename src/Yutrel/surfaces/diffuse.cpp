#include "diffuse.h"

#include "base/renderer.h"
#include "base/scene.h"
#include "utils/sampling.h"

namespace Yutrel
{
Diffuse::Diffuse(Scene& scene, const CreateInfo& info) noexcept
    : Surface(scene, info), m_reflectance(scene.load_texture(info.reflectance)) {}

luisa::unique_ptr<Surface::Instance> Diffuse::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto reflectance = renderer.build_texture(command_buffer, m_reflectance);

    return luisa::make_unique<Instance>(renderer, this, reflectance);
}

luisa::unique_ptr<Surface::Closure> Diffuse::Instance::create_closure(Expr<float> time) const noexcept
{
    return luisa::make_unique<Closure>(renderer(), time);
}

void Diffuse::Instance::populate_closure(Surface::Closure* closure, const Interaction& it) const noexcept
{
    Float3 reflectance = m_reflectance->evaluate(it).xyz();

    Diffuse::Closure::Context ctx{
        .it          = it,
        .reflectance = reflectance,
    };
    closure->bind(std::move(ctx));
}

Bool Diffuse::Closure::scatter(Expr<float3> wo, Var<Ray>& scattered, Var<float3>& attenuation, Var<float>& pdf, Expr<float2> u, Expr<float> u_lobe) const noexcept
{
    auto&& ctx = context<Context>();

    auto scatter_direction = ctx.it.shading.local_to_world(sample_cosine_hemisphere(u));

    scattered   = make_ray(ctx.it.p_s + ctx.it.shading.n() * 1e-4f, normalize(scatter_direction));
    attenuation = ctx.reflectance;
    pdf         = dot(ctx.it.shading.n(), scatter_direction) / pi;

    return true;
}

Float Diffuse::Closure::scatter_pdf(Expr<float3> wo, Expr<float3> wi, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept
{
    auto&& ctx     = context<Context>();
    auto cos_theta = dot(ctx.it.shading.n(), wi);
    return ite(cos_theta > 0.0f, cos_theta / pi, 0.0f);
}

} // namespace Yutrel