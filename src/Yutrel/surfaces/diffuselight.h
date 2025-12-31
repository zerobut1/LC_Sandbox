#pragma once

#include "base/interaction.h"
#include "base/surface.h"

namespace Yutrel
{
class DiffuseLight : public Surface
{
public:
    class Instance;
    class Closure;

private:
    const Texture* m_emit;

public:
    explicit DiffuseLight(Scene& scene, const CreateInfo& info) noexcept;

public:
    [[nodiscard]] luisa::unique_ptr<Surface::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
};

class DiffuseLight::Instance : public Surface::Instance
{
private:
    const Texture::Instance* m_emit;

public:
    explicit Instance(const Renderer& renderer, const DiffuseLight* surface, const Texture::Instance* emit) noexcept
        : Surface::Instance(renderer, surface), m_emit(emit) {}

public:
    [[nodiscard]] luisa::string closure_identifier() const noexcept override { return "DiffuseLight"; }
    [[nodiscard]] luisa::unique_ptr<Surface::Closure> create_closure(Expr<float> time) const noexcept override;
    void populate_closure(Surface::Closure* closure, const Interaction& it) const noexcept override;
};

class DiffuseLight::Closure : public Surface::Closure
{
public:
    struct Context
    {
        Interaction it;
        Float3 emit;
    };

public:
    using Surface::Closure::Closure;

    [[nodiscard]] Bool scatter(Var<Ray>& ray, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override;
    [[nodiscard]] Float3 emitted() const noexcept override;
};

} // namespace Yutrel