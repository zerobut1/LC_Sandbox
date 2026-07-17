#include "coated_diffuse.h"

#include <limits>

#include <luisa/dsl/sugar.h>

#include "base/renderer.h"
#include "scene/scene_builder.h"
#include "utils/rng.h"
#include "utils/sampling.h"
#include "utils/scattering.h"

namespace Yutrel
{
// The coated random-walk estimator is adapted from PBRT-v4's LayeredBxDF
// (Apache-2.0), specialized here to a dielectric coat over Lambertian reflection.
namespace
{
struct InterfaceEvaluation
{
    SampledSpectrum f;
    Float pdf;
};

struct InterfaceSample
{
    InterfaceEvaluation eval;
    Float3 wi;
    Bool valid;
    Bool reflection;
};

enum class InterfaceSampleMode
{
    ALL,
    REFLECTION,
    TRANSMISSION,
};

class DielectricInterface
{
private:
    uint m_dimension;
    Float m_eta;
    TrowbridgeReitzDistribution m_distribution;
    FresnelDielectric m_fresnel;
    SampledSpectrum m_unit;
    MicrofacetReflection m_reflection;
    MicrofacetTransmission m_transmission;

public:
    DielectricInterface(uint dimension, Expr<float2> alpha, Expr<float> eta) noexcept
        : m_dimension{dimension},
          m_eta{eta},
          m_distribution{alpha},
          m_fresnel{1.0f, eta},
          m_unit{dimension, 1.0f},
          m_reflection{m_unit, std::addressof(m_distribution), std::addressof(m_fresnel)},
          m_transmission{m_unit, std::addressof(m_distribution), 1.0f, eta} {}

    [[nodiscard]] Bool smooth() const noexcept { return m_distribution.effectively_smooth(); }

    [[nodiscard]] InterfaceEvaluation evaluate(Expr<float3> wo, Expr<float3> wi,
                                               TransportMode mode,
                                               InterfaceSampleMode sample_mode = InterfaceSampleMode::ALL) const noexcept
    {
        InterfaceEvaluation e{SampledSpectrum{m_dimension}, 0.0f};
        $if(!smooth())
        {
            auto reflection = same_hemisphere(wo, wi);
            $if(reflection)
            {
                if (sample_mode != InterfaceSampleMode::TRANSMISSION)
                {
                    e.f   = m_reflection.evaluate(wo, wi, mode) * abs_cos_theta(wi);
                    e.pdf = m_reflection.pdf(wo, wi, mode);
                    if (sample_mode == InterfaceSampleMode::ALL)
                    {
                        auto wh = normalize(wo + wi);
                        e.pdf *= m_fresnel.evaluate(dot(wo, wh));
                    }
                }
            }
            $else
            {
                if (sample_mode != InterfaceSampleMode::REFLECTION)
                {
                    e.f   = m_transmission.evaluate(wo, wi, mode) * abs_cos_theta(wi);
                    e.pdf = m_transmission.pdf(wo, wi, mode);
                    if (sample_mode == InterfaceSampleMode::ALL)
                    {
                        auto etap = ite(cos_theta(wo) > 0.0f, m_eta, 1.0f / m_eta);
                        auto wh   = normalize(wo + wi * etap);
                        wh        = ite(wh.z < 0.0f, -wh, wh);
                        e.pdf *= 1.0f - m_fresnel.evaluate(dot(wo, wh));
                    }
                }
            };
        };
        return e;
    }

    [[nodiscard]] InterfaceSample sample(Expr<float3> wo, Expr<float> uc, Expr<float2> u,
                                         TransportMode mode,
                                         InterfaceSampleMode sample_mode = InterfaceSampleMode::ALL) const noexcept
    {
        InterfaceSample s{{SampledSpectrum{m_dimension}, 0.0f}, make_float3(0.0f), false, false};
        $if(smooth())
        {
            auto F          = m_fresnel.evaluate(cos_theta(wo));
            auto choose_refl = sample_mode == InterfaceSampleMode::REFLECTION ? Bool{true} :
                               sample_mode == InterfaceSampleMode::TRANSMISSION ? Bool{false} : uc < F;
            $if(choose_refl)
            {
                auto wi      = make_float3(-wo.x, -wo.y, wo.z);
                auto p       = sample_mode == InterfaceSampleMode::ALL ? F : 1.0f;
                s.eval.f     = m_unit * F;
                s.eval.pdf   = p;
                s.wi         = wi;
                s.valid      = p > 0.0f;
                s.reflection = true;
            }
            $else
            {
                auto entering = cos_theta(wo) > 0.0f;
                auto n        = make_float3(0.0f, 0.0f, ite(entering, 1.0f, -1.0f));
                auto eta      = ite(entering, 1.0f / m_eta, m_eta);
                auto wi       = def(make_float3(0.0f));
                auto ok       = refract(wo, n, eta, std::addressof(wi));
                auto p        = sample_mode == InterfaceSampleMode::ALL ? 1.0f - F : 1.0f;
                auto ft       = m_unit * (1.0f - F);
                if (mode == TransportMode::RADIANCE)
                {
                    ft *= sqr(eta);
                }
                s.eval.f     = ite(ok, ft, 0.0f);
                s.eval.pdf   = ite(ok, p, 0.0f);
                s.wi         = wi;
                s.valid      = ok & (p > 0.0f);
                s.reflection = false;
            };
        }
        $else
        {
            auto wh = m_distribution.sample_wh(wo, u);
            auto F  = m_fresnel.evaluate(dot(wo, wh));
            auto choose_refl = sample_mode == InterfaceSampleMode::REFLECTION ? Bool{true} :
                               sample_mode == InterfaceSampleMode::TRANSMISSION ? Bool{false} : uc < F;
            $if(choose_refl)
            {
                auto d       = m_reflection.sample_wi(wo, u, mode);
                auto p       = m_reflection.pdf(wo, d.wi, mode);
                auto select  = sample_mode == InterfaceSampleMode::ALL ? F : 1.0f;
                s.eval.f     = m_reflection.evaluate(wo, d.wi, mode) * abs_cos_theta(d.wi);
                s.eval.pdf   = ite(d.valid, p * select, 0.0f);
                s.wi         = d.wi;
                s.valid      = d.valid & (s.eval.pdf > 0.0f);
                s.reflection = true;
            }
            $else
            {
                auto d       = m_transmission.sample_wi(wo, u, mode);
                auto p       = m_transmission.pdf(wo, d.wi, mode);
                auto select  = sample_mode == InterfaceSampleMode::ALL ? 1.0f - F : 1.0f;
                s.eval.f     = m_transmission.evaluate(wo, d.wi, mode) * abs_cos_theta(d.wi);
                s.eval.pdf   = ite(d.valid, p * select, 0.0f);
                s.wi         = d.wi;
                s.valid      = d.valid & (s.eval.pdf > 0.0f);
                s.reflection = false;
            };
        };
        return s;
    }
};

class HGPhaseFunction
{
private:
    Float m_g;

public:
    struct Sample
    {
        Float p;
        Float3 wi;
        Float pdf;
    };

    explicit HGPhaseFunction(Expr<float> g) noexcept : m_g{g} {}

    [[nodiscard]] Float p(Expr<float3> wo, Expr<float3> wi) const noexcept
    {
        auto denom = 1.0f + sqr(m_g) + 2.0f * m_g * dot(wo, wi);
        return 0.25f * inv_pi * (1.0f - sqr(m_g)) / (denom * sqrt(denom));
    }

    [[nodiscard]] Sample sample(Expr<float3> wo, Expr<float2> u) const noexcept
    {
        auto cos_theta = ite(abs(m_g) < 1e-3f,
                             1.0f - 2.0f * u.x,
                             -0.5f / m_g *
                                 (1.0f + sqr(m_g) -
                                  sqr((1.0f - sqr(m_g)) / (1.0f + m_g - 2.0f * m_g * u.x))));
        auto sin_theta = sqrt(max(0.0f, 1.0f - sqr(cos_theta)));
        auto phi       = 2.0f * pi * u.y;
        auto wi        = Frame::make(wo).local_to_world(
            make_float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta));
        auto pdf = p(wo, wi);
        return {pdf, wi, pdf};
    }
};

[[nodiscard]] inline Float transmittance(Expr<float> dz, Expr<float3> w) noexcept
{
    return ite(abs(dz) <= std::numeric_limits<float>::min(), 1.0f, exp(-abs(dz / w.z)));
}
} // namespace

class CoatedDiffuse::Closure::Impl
{
private:
    const Context& m_ctx;
    LambertianReflection m_substrate;
    DielectricInterface m_coat;

    [[nodiscard]] InterfaceEvaluation substrate_evaluate(Expr<float3> wo, Expr<float3> wi,
                                                          TransportMode mode) const noexcept
    {
        return {m_substrate.evaluate(wo, wi, mode) * abs_cos_theta(wi),
                m_substrate.pdf(wo, wi, mode)};
    }

    [[nodiscard]] InterfaceSample substrate_sample(Expr<float3> wo, Expr<float2> u,
                                                   TransportMode mode) const noexcept
    {
        auto wi  = def(make_float3(0.0f));
        auto pdf = def(0.0f);
        auto f   = m_substrate.sample(wo, std::addressof(wi), u, std::addressof(pdf), mode);
        return {{f * abs_cos_theta(wi), pdf}, wi, pdf > 0.0f, true};
    }

public:
    explicit Impl(const Context& ctx) noexcept
        : m_ctx{ctx}, m_substrate{ctx.reflectance}, m_coat{ctx.reflectance.dimension(), ctx.alpha, ctx.eta} {}

    [[nodiscard]] InterfaceEvaluation evaluate(Expr<float3> wo, Expr<float3> wi) const noexcept
    {
        InterfaceEvaluation result{SampledSpectrum{m_ctx.reflectance.dimension()}, 0.0f};
        $if(same_hemisphere(wo, wi))
        {
            auto direct = m_coat.evaluate(wo, wi, TransportMode::RADIANCE);
            result.f    = direct.f * Float{m_ctx.samples};
            auto seed   = xxhash32(make_uint4(as<UInt3>(m_ctx.it.p_g), xxhash32(as<UInt3>(wi))));
            auto pdf_sum = direct.pdf * Float{m_ctx.samples};
            HGPhaseFunction phase{m_ctx.g};

            $for(sample_index, m_ctx.samples)
            {
                auto wos = m_coat.sample(wo, lcg(seed), make_float2(lcg(seed), lcg(seed)),
                                         TransportMode::RADIANCE, InterfaceSampleMode::TRANSMISSION);
                auto wis = m_coat.sample(wi, lcg(seed), make_float2(lcg(seed), lcg(seed)),
                                         TransportMode::IMPORTANCE, InterfaceSampleMode::TRANSMISSION);
                $if(!wos.valid | !wis.valid) { $continue; };

                auto beta = wos.eval.f / wos.eval.pdf;
                auto z    = def(m_ctx.thickness);
                auto w    = def(wos.wi);
                $for(depth, m_ctx.max_depth)
                {
                    $if((depth > 3u) & (beta.max() < 0.25f))
                    {
                        auto q = max(0.0f, 1.0f - beta.max());
                        $if(lcg(seed) < q) { $break; };
                        beta /= 1.0f - q;
                    };
                    $if(m_ctx.medium_albedo.is_zero())
                    {
                        z = ite(z == m_ctx.thickness, 0.0f, m_ctx.thickness);
                        beta *= transmittance(m_ctx.thickness, w);
                    }
                    $else
                    {
                        auto dz = sample_exponential(lcg(seed), 1.0f / abs(w.z));
                        auto zp = ite(w.z > 0.0f, z + dz, z - dz);
                        $if((zp > 0.0f) & (zp < m_ctx.thickness))
                        {
                            auto phase_pdf = phase.p(-w, -wis.wi);
                            auto exit_pdf  = m_coat.evaluate(-w, wi, TransportMode::RADIANCE).pdf;
                            auto wt        = power_heuristic(wis.eval.pdf, exit_pdf);
                            result.f += beta * m_ctx.medium_albedo * phase_pdf * wt *
                                        transmittance(zp - m_ctx.thickness, wis.wi) * wis.eval.f / wis.eval.pdf;
                            auto ps = phase.sample(-w, make_float2(lcg(seed), lcg(seed)));
                            $if((ps.pdf <= 0.0f) | (ps.wi.z == 0.0f)) { $continue; };
                            beta *= m_ctx.medium_albedo * ps.p / ps.pdf;
                            w = ps.wi;
                            z = zp;
                            $if(w.z > 0.0f)
                            {
                                auto exit = m_coat.evaluate(-w, wi, TransportMode::RADIANCE,
                                                            InterfaceSampleMode::TRANSMISSION);
                                auto exit_wt = power_heuristic(ps.pdf, exit.pdf);
                                result.f += beta * transmittance(zp - m_ctx.thickness, w) * exit.f * exit_wt;
                            };
                            $continue;
                        };
                        z = clamp(zp, 0.0f, m_ctx.thickness);
                    };

                    $if(z == m_ctx.thickness)
                    {
                        auto bs = m_coat.sample(-w, lcg(seed), make_float2(lcg(seed), lcg(seed)),
                                                TransportMode::RADIANCE, InterfaceSampleMode::REFLECTION);
                        $if(!bs.valid) { $break; };
                        beta *= bs.eval.f / bs.eval.pdf;
                        w = bs.wi;
                    }
                    $else
                    {
                        auto nee = substrate_evaluate(-w, -wis.wi, TransportMode::RADIANCE);
                        auto wt  = power_heuristic(wis.eval.pdf, nee.pdf);
                        result.f += beta * nee.f * wt * transmittance(m_ctx.thickness, wis.wi) *
                                    wis.eval.f / wis.eval.pdf;
                        auto bs = substrate_sample(-w, make_float2(lcg(seed), lcg(seed)), TransportMode::RADIANCE);
                        $if(!bs.valid) { $break; };
                        beta *= bs.eval.f / bs.eval.pdf;
                        w = bs.wi;
                        auto exit = m_coat.evaluate(-w, wi, TransportMode::RADIANCE,
                                                    InterfaceSampleMode::TRANSMISSION);
                        auto exit_wt = power_heuristic(bs.eval.pdf, exit.pdf);
                        result.f += beta * transmittance(m_ctx.thickness, w) * exit.f * exit_wt;
                    };
                };
            };

            $for(sample_index, m_ctx.samples)
            {
                auto wos = m_coat.sample(wo, lcg(seed), make_float2(lcg(seed), lcg(seed)),
                                         TransportMode::RADIANCE, InterfaceSampleMode::TRANSMISSION);
                auto wis = m_coat.sample(wi, lcg(seed), make_float2(lcg(seed), lcg(seed)),
                                         TransportMode::IMPORTANCE, InterfaceSampleMode::TRANSMISSION);
                $if(!wos.valid | !wis.valid) { $continue; };
                $if(m_coat.smooth())
                {
                    pdf_sum += substrate_evaluate(-wos.wi, -wis.wi, TransportMode::RADIANCE).pdf;
                }
                $else
                {
                    auto rs = substrate_sample(-wos.wi, make_float2(lcg(seed), lcg(seed)),
                                               TransportMode::RADIANCE);
                    $if(rs.valid)
                    {
                        auto r_pdf = substrate_evaluate(-wos.wi, -wis.wi, TransportMode::RADIANCE).pdf;
                        pdf_sum += power_heuristic(wis.eval.pdf, r_pdf) * r_pdf;
                        auto t_pdf = m_coat.evaluate(-rs.wi, wi, TransportMode::RADIANCE,
                                                    InterfaceSampleMode::TRANSMISSION).pdf;
                        pdf_sum += power_heuristic(rs.eval.pdf, t_pdf) * t_pdf;
                    };
                };
            };
            result.f /= Float{m_ctx.samples};
            result.pdf = lerp(0.25f * inv_pi, pdf_sum / Float{m_ctx.samples}, 0.9f);
        };
        return result;
    }

    [[nodiscard]] InterfaceSample sample(Expr<float3> wo, Expr<float> u_lobe, Expr<float2> u) const noexcept
    {
        auto bs = m_coat.sample(wo, u_lobe, u, TransportMode::RADIANCE);
        InterfaceSample result{{SampledSpectrum{m_ctx.reflectance.dimension()}, 0.0f},
                               make_float3(0.0f), false, true};
        $if(bs.valid)
        {
            $if(bs.reflection)
            {
                result = bs;
            }
            $else
            {
                auto w    = def(bs.wi);
                auto z    = def(m_ctx.thickness);
                auto f    = bs.eval.f;
                auto pdf  = def(bs.eval.pdf);
                auto seed = xxhash32(make_uint4(as<UInt3>(make_float3(u, u_lobe)), xxhash32(as<UInt3>(wo))));
                HGPhaseFunction phase{m_ctx.g};
                $for(depth, m_ctx.max_depth)
                {
                    auto rr_beta = f.max() / pdf;
                    $if((depth > 3u) & (rr_beta < 0.25f))
                    {
                        auto q = max(0.0f, 1.0f - rr_beta);
                        $if(lcg(seed) < q) { $break; };
                        pdf *= 1.0f - q;
                    };
                    $if(w.z == 0.0f) { $break; };
                    $if(!m_ctx.medium_albedo.is_zero())
                    {
                        auto dz = sample_exponential(lcg(seed), 1.0f / abs(w.z));
                        auto zp = ite(w.z > 0.0f, z + dz, z - dz);
                        $if((zp > 0.0f) & (zp < m_ctx.thickness))
                        {
                            auto ps = phase.sample(-w, make_float2(lcg(seed), lcg(seed)));
                            $if(ps.pdf <= 0.0f) { $break; };
                            f *= m_ctx.medium_albedo * ps.p;
                            pdf *= ps.pdf;
                            w = ps.wi;
                            z = zp;
                            $continue;
                        };
                        z = clamp(zp, 0.0f, m_ctx.thickness);
                    }
                    $else
                    {
                        z = ite(z == m_ctx.thickness, 0.0f, m_ctx.thickness);
                        f *= transmittance(m_ctx.thickness, w);
                    };

                    InterfaceSample next{{SampledSpectrum{m_ctx.reflectance.dimension()}, 0.0f},
                                         make_float3(0.0f), false, true};
                    $if(z == 0.0f)
                    {
                        next = substrate_sample(-w, make_float2(lcg(seed), lcg(seed)), TransportMode::RADIANCE);
                    }
                    $else
                    {
                        next = m_coat.sample(-w, lcg(seed), make_float2(lcg(seed), lcg(seed)),
                                             TransportMode::RADIANCE);
                    };
                    $if(!next.valid) { $break; };
                    f *= next.eval.f;
                    pdf *= next.eval.pdf;
                    w = next.wi;
                    $if((z == m_ctx.thickness) & !next.reflection)
                    {
                        result = {{f, pdf}, w, true, true};
                        $break;
                    };
                };
            };
        };
        return result;
    }
};

CoatedDiffuse::Closure::Closure(const Renderer& renderer, const SampledWavelengths& swl,
                                Expr<float> time) noexcept
    : Surface::Closure{renderer, swl, time} {}

CoatedDiffuse::Closure::~Closure() noexcept = default;

CoatedDiffuse::CoatedDiffuse(const Texture* reflectance, const Texture* roughness,
                             const Texture* u_roughness, const Texture* v_roughness,
                             const Texture* thickness, const Texture* albedo,
                             const Texture* g, const Texture* eta,
                             bool remap_roughness, uint max_depth, uint samples) noexcept
    : Surface{true}, m_reflectance{reflectance}, m_roughness{roughness},
      m_u_roughness{u_roughness}, m_v_roughness{v_roughness}, m_thickness{thickness},
      m_albedo{albedo}, m_g{g}, m_eta{eta}, m_remap_roughness{remap_roughness},
      m_max_depth{max_depth}, m_samples{samples} {}

luisa::unique_ptr<Surface::Instance> CoatedDiffuse::build(Renderer& renderer,
                                                          CommandBuffer& command_buffer) const noexcept
{
    return luisa::make_unique<Instance>(
        renderer, this,
        renderer.build_texture(command_buffer, m_reflectance),
        renderer.build_texture(command_buffer, m_roughness),
        renderer.build_texture(command_buffer, m_u_roughness),
        renderer.build_texture(command_buffer, m_v_roughness),
        renderer.build_texture(command_buffer, m_thickness),
        renderer.build_texture(command_buffer, m_albedo),
        renderer.build_texture(command_buffer, m_g),
        renderer.build_texture(command_buffer, m_eta));
}

CoatedDiffuse::Instance::Instance(const Renderer& renderer, const CoatedDiffuse* surface,
                                  const Texture::Instance* reflectance,
                                  const Texture::Instance* roughness,
                                  const Texture::Instance* u_roughness,
                                  const Texture::Instance* v_roughness,
                                  const Texture::Instance* thickness,
                                  const Texture::Instance* albedo,
                                  const Texture::Instance* g,
                                  const Texture::Instance* eta) noexcept
    : Surface::Instance{renderer, surface}, m_reflectance{reflectance}, m_roughness{roughness},
      m_u_roughness{u_roughness}, m_v_roughness{v_roughness}, m_thickness{thickness},
      m_albedo{albedo}, m_g{g}, m_eta{eta} {}

luisa::unique_ptr<Surface::Closure> CoatedDiffuse::Instance::create_closure(
    SampledWavelengths& swl, Expr<float> time) const noexcept
{
    return luisa::make_unique<Closure>(renderer(), swl, time);
}

void CoatedDiffuse::Instance::populate_closure(Surface::Closure* closure,
                                               const Interaction& it) const noexcept
{
    auto& swl = closure->swl();
    auto time = closure->time();
    auto dimension = swl.dimension();
    auto reflectance = m_reflectance ?
                           m_reflectance->evaluate_albedo_spectrum(it, swl, time).value :
                           SampledSpectrum{dimension, 0.5f};
    auto roughness = m_roughness ? m_roughness->evaluate(it, time).x : Float{0.0f};
    auto uroughness = m_u_roughness ? m_u_roughness->evaluate(it, time).x : roughness;
    auto vroughness = m_v_roughness ? m_v_roughness->evaluate(it, time).x : roughness;
    auto alpha = make_float2(uroughness, vroughness);
    if (base<CoatedDiffuse>()->remap_roughness())
    {
        alpha = TrowbridgeReitzDistribution::roughness_to_alpha(alpha);
    }
    auto thickness = m_thickness ? max(m_thickness->evaluate(it, time).x,
                                       std::numeric_limits<float>::min()) : Float{0.01f};
    auto medium_albedo = m_albedo ?
                             m_albedo->evaluate_albedo_spectrum(it, swl, time).value :
                             SampledSpectrum{dimension};
    auto g   = m_g ? clamp(m_g->evaluate(it, time).x, -1.0f, 1.0f) : Float{0.0f};
    auto eta = m_eta ? m_eta->evaluate(it, time).x : Float{1.5f};
    eta      = ite(eta == 0.0f, 1.0f, eta);
    closure->bind(Closure::Context{
        .it = it,
        .reflectance = reflectance,
        .alpha = alpha,
        .thickness = thickness,
        .medium_albedo = medium_albedo,
        .g = g,
        .eta = eta,
        .max_depth = base<CoatedDiffuse>()->max_depth(),
        .samples = base<CoatedDiffuse>()->samples(),
    });
}

void CoatedDiffuse::Closure::pre_eval() noexcept
{
    m_impl = luisa::make_unique<Impl>(context<Context>());
}

void CoatedDiffuse::Closure::post_eval() noexcept
{
    m_impl = nullptr;
}

Surface::Sample CoatedDiffuse::Closure::sample_impl(Expr<float3> wo, Expr<float> u_lobe,
                                                     Expr<float2> u) const noexcept
{
    auto&& ctx = context<Context>();
    auto wo_local = ctx.it.shading.world_to_local(wo);
    auto s = m_impl->sample(wo_local, u_lobe, u);
    return Surface::Sample{
        .eval = {
            .f = s.eval.f,
            .pdf = s.eval.pdf,
            .f_diffuse = SampledSpectrum{swl().dimension()},
            .pdf_diffuse = 0.0f,
        },
        .wi = ctx.it.shading.local_to_world(s.wi),
        .event = Surface::event_reflect,
        .eta = 1.0f,
    };
}

Surface::Evaluation CoatedDiffuse::Closure::evaluate_impl(Expr<float3> wo,
                                                           Expr<float3> wi) const noexcept
{
    auto&& ctx = context<Context>();
    auto e = m_impl->evaluate(ctx.it.shading.world_to_local(wo),
                              ctx.it.shading.world_to_local(wi));
    return {
        .f = e.f,
        .pdf = e.pdf,
        .f_diffuse = SampledSpectrum{swl().dimension()},
        .pdf_diffuse = 0.0f,
    };
}

luisa::optional<luisa::string> CoatedDiffuseSurfaceSpec::validate() const noexcept
{
    if (m_params.max_depth == 0u) { return spec_validation_error("CoatedDiffuse max_depth must be positive."); }
    if (m_params.samples == 0u) { return spec_validation_error("CoatedDiffuse samples must be positive."); }
    return luisa::nullopt;
}

void CoatedDiffuseSurfaceSpec::visit_dependencies(SpecDependencyVisitor& visitor) const noexcept
{
    auto visit = [&visitor](const luisa::optional<TextureRef>& ref) noexcept
    {
        if (ref) { visitor.visit(*ref); }
    };
    visit(m_params.reflectance);
    visit(m_params.roughness);
    visit(m_params.u_roughness);
    visit(m_params.v_roughness);
    visit(m_params.thickness);
    visit(m_params.albedo);
    visit(m_params.g);
    visit(m_params.eta);
}

const Surface* CoatedDiffuseSurfaceSpec::build(SceneBuilder& builder) const noexcept
{
    auto resolve = [&builder](const luisa::optional<TextureRef>& ref) noexcept -> const Texture*
    {
        return ref ? builder.resolve(*ref) : nullptr;
    };
    return builder.emplace<Surface, CoatedDiffuse>(
        resolve(m_params.reflectance), resolve(m_params.roughness),
        resolve(m_params.u_roughness), resolve(m_params.v_roughness),
        resolve(m_params.thickness), resolve(m_params.albedo),
        resolve(m_params.g), resolve(m_params.eta), m_params.remap_roughness,
        m_params.max_depth, m_params.samples);
}
} // namespace Yutrel
