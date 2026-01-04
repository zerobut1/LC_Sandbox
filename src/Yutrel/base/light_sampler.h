#pragma once

#include <luisa/core/stl.h>
#include <luisa/dsl/syntax.h>

#include "base/light.h"
#include "utils/command_buffer.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Scene;
class Renderer;
class Interaction;

class LightSampler
{
public:
    [[nodiscard]] static luisa::unique_ptr<LightSampler> create(Renderer& renderer, CommandBuffer& command_buffer) noexcept;

public:
    struct Selection
    {
        UInt tag;
        Float prob;
    };

    using Evaluation = Light::Evaluation;

    struct Sample
    {
        Evaluation eval;
        Var<Ray> shadow_ray;
        [[nodiscard]] static Sample zero() noexcept;
        [[nodiscard]] static Sample from_light(const Light::Sample& s, const Interaction& it_from) noexcept;
    };

private:
    const Renderer& m_renderer;

    uint m_light_buffer_id{0u};

public:
    explicit LightSampler(Renderer& renderer, CommandBuffer& command_buffer) noexcept;
    ~LightSampler() noexcept = default;

    LightSampler()                               = delete;
    LightSampler(const LightSampler&)            = delete;
    LightSampler& operator=(const LightSampler&) = delete;
    LightSampler(LightSampler&&)                 = delete;
    LightSampler& operator=(LightSampler&&)      = delete;

public:
    [[nodiscard]] auto& renderer() const noexcept { return m_renderer; }

    [[nodiscard]] Evaluation evaluate_hit(const Interaction& it, Expr<float3> p_from, Expr<float> time) const noexcept;
    [[nodiscard]] Sample sample(const Interaction& it_from, Expr<float> u_select, Expr<float2> u_light, Expr<float> time) const noexcept;
    [[nodiscard]] Selection select(const Interaction& it_from, Expr<float> u, Expr<float> time) const noexcept;
    [[nodiscard]] Sample sample_selection(const Interaction& it_from, const Selection& sel, Expr<float2> u, Expr<float> time) const noexcept;
    [[nodiscard]] auto sample_area(Expr<float3> p_from, Expr<uint> tag, Expr<float2> u_in) const noexcept;
    [[nodiscard]] Sample sample_light(const Interaction& it_from, const Selection& sel, Expr<float2> u, Expr<float> time) const noexcept;
};

} // namespace Yutrel