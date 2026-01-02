#pragma once

#include "base/interaction.h"
#include "base/surface.h"

namespace Yutrel
{
class Texture;

class Diffuse : public Surface
{
public:
    class Instance;
    class Closure;

private:
    const Texture* m_reflectance;

public:
    explicit Diffuse(Scene& scene, const CreateInfo& info) noexcept;

public:
    [[nodiscard]] luisa::unique_ptr<Surface::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
};

class Diffuse::Instance : public Surface::Instance
{
private:
    const Texture::Instance* m_reflectance;

public:
    explicit Instance(const Renderer& renderer, const Diffuse* surface, const Texture::Instance* reflectance) noexcept
        : Surface::Instance(renderer, surface), m_reflectance(reflectance) {}

public:
    [[nodiscard]] luisa::string closure_identifier() const noexcept override { return "Diffuse"; }
    [[nodiscard]] luisa::unique_ptr<Surface::Closure> create_closure(Expr<float> time) const noexcept override;
    void populate_closure(Surface::Closure* closure, const Interaction& it) const noexcept override;
};

class Diffuse::Closure : public Surface::Closure
{
public:
    struct Context
    {
        Interaction it;
        Float3 reflectance;
    };

public:
    using Surface::Closure::Closure;

    [[nodiscard]] Bool scatter(Var<Ray>& ray, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override;
    [[nodiscard]] Float scatter_pdf(Expr<float3> wo, Expr<float3> wi, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override;
};

} // namespace Yutrel