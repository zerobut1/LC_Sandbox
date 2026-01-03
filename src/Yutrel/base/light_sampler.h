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
    using Evaluation = Light::Evaluation;

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
};

} // namespace Yutrel