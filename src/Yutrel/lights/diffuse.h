#pragma once

#include "base/light.h"
#include "base/texture.h"

namespace Yutrel
{
class DiffuseLight : public Light
{
public:
    class Instance;
    class Closure;

private:
    const Texture* m_emission;
    float m_scale;
    bool m_two_sided;

public:
    explicit DiffuseLight(Scene& scene, const CreateInfo& info) noexcept;

    [[nodiscard]] auto scale() const noexcept { return m_scale; }
    [[nodiscard]] auto two_sided() const noexcept { return m_two_sided; }
    [[nodiscard]] bool is_null() const noexcept override { return m_scale == 0.0f; }
    [[nodiscard]] luisa::unique_ptr<Light::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
};

class DiffuseLight::Instance : public Light::Instance
{
private:
    const Texture::Instance* m_emission;

public:
    explicit Instance(const Renderer& renderer, const DiffuseLight* light, const Texture::Instance* emission) noexcept
        : Light::Instance(renderer, light), m_emission(emission) {}

    [[nodiscard]] auto texture() const noexcept { return m_emission; }
    [[nodiscard]] luisa::unique_ptr<Light::Closure> closure(Expr<float> time) const noexcept override;
};

class DiffuseLight::Closure : public Light::Closure
{
public:
    Closure(const Light::Instance* instance, Expr<float> time) noexcept
        : Light::Closure(instance, time) {}

    [[nodiscard]] Evaluation evaluate(const Interaction& it_light, Expr<float3> p_from) const noexcept;
};

} // namespace Yutrel