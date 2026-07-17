#pragma once

#include <utility>

#include <luisa/core/stl/optional.h>

#include "base/interaction.h"
#include "base/surface.h"
#include "scene/spec_base.h"

namespace Yutrel
{
class Texture;

struct CoatedDiffuseSurfaceParams
{
    luisa::optional<TextureRef> reflectance;
    luisa::optional<TextureRef> roughness;
    luisa::optional<TextureRef> u_roughness;
    luisa::optional<TextureRef> v_roughness;
    luisa::optional<TextureRef> thickness;
    luisa::optional<TextureRef> albedo;
    luisa::optional<TextureRef> g;
    luisa::optional<TextureRef> eta;
    bool remap_roughness{true};
    uint max_depth{10u};
    uint samples{1u};
};

class CoatedDiffuse final : public Surface
{
public:
    class Instance;
    class Closure;

private:
    const Texture* m_reflectance;
    const Texture* m_roughness;
    const Texture* m_u_roughness;
    const Texture* m_v_roughness;
    const Texture* m_thickness;
    const Texture* m_albedo;
    const Texture* m_g;
    const Texture* m_eta;
    bool m_remap_roughness;
    uint m_max_depth;
    uint m_samples;

public:
    CoatedDiffuse(const Texture* reflectance,
                  const Texture* roughness,
                  const Texture* u_roughness,
                  const Texture* v_roughness,
                  const Texture* thickness,
                  const Texture* albedo,
                  const Texture* g,
                  const Texture* eta,
                  bool remap_roughness,
                  uint max_depth,
                  uint samples) noexcept;

    [[nodiscard]] bool remap_roughness() const noexcept { return m_remap_roughness; }
    [[nodiscard]] uint max_depth() const noexcept { return m_max_depth; }
    [[nodiscard]] uint samples() const noexcept { return m_samples; }
    [[nodiscard]] luisa::unique_ptr<Surface::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
};

class CoatedDiffuse::Instance final : public Surface::Instance
{
private:
    const Texture::Instance* m_reflectance;
    const Texture::Instance* m_roughness;
    const Texture::Instance* m_u_roughness;
    const Texture::Instance* m_v_roughness;
    const Texture::Instance* m_thickness;
    const Texture::Instance* m_albedo;
    const Texture::Instance* m_g;
    const Texture::Instance* m_eta;

public:
    Instance(const Renderer& renderer,
             const CoatedDiffuse* surface,
             const Texture::Instance* reflectance,
             const Texture::Instance* roughness,
             const Texture::Instance* u_roughness,
             const Texture::Instance* v_roughness,
             const Texture::Instance* thickness,
             const Texture::Instance* albedo,
             const Texture::Instance* g,
             const Texture::Instance* eta) noexcept;

    [[nodiscard]] luisa::string closure_identifier() const noexcept override { return "CoatedDiffuse"; }
    [[nodiscard]] luisa::unique_ptr<Surface::Closure> create_closure(SampledWavelengths& swl, Expr<float> time) const noexcept override;
    void populate_closure(Surface::Closure* closure, const Interaction& it) const noexcept override;
};

class CoatedDiffuse::Closure final : public Surface::Closure
{
public:
    struct Context
    {
        Interaction it;
        SampledSpectrum reflectance;
        Float2 alpha;
        Float thickness;
        SampledSpectrum medium_albedo;
        Float g;
        Float eta;
        UInt max_depth;
        UInt samples;
    };

private:
    class Impl;
    luisa::unique_ptr<Impl> m_impl;

public:
    Closure(const Renderer& renderer, const SampledWavelengths& swl, Expr<float> time) noexcept;
    ~Closure() noexcept override;

    [[nodiscard]] const Interaction& it() const noexcept override { return context<Context>().it; }
    void pre_eval() noexcept override;
    void post_eval() noexcept override;

private:
    [[nodiscard]] Surface::Sample sample_impl(Expr<float3> wo, Expr<float> u_lobe, Expr<float2> u) const noexcept override;
    [[nodiscard]] Surface::Evaluation evaluate_impl(Expr<float3> wo, Expr<float3> wi) const noexcept override;
};

class CoatedDiffuseSurfaceSpec final : public SurfaceSpec
{
private:
    CoatedDiffuseSurfaceParams m_params;

public:
    explicit CoatedDiffuseSurfaceSpec(CoatedDiffuseSurfaceParams params) noexcept
        : m_params{std::move(params)} {}

    [[nodiscard]] const CoatedDiffuseSurfaceParams& params() const noexcept { return m_params; }
    [[nodiscard]] luisa::optional<luisa::string> validate() const noexcept override;
    void visit_dependencies(SpecDependencyVisitor& visitor) const noexcept override;
    [[nodiscard]] const Surface* build(SceneBuilder& builder) const noexcept override;
};

} // namespace Yutrel
