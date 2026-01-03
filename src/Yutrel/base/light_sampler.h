#pragma once

#include <luisa/core/stl.h>
#include <luisa/dsl/syntax.h>

#include "utils/command_buffer.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Scene;
class Renderer;

class LightSampler
{
public:
    [[nodiscard]] static luisa::unique_ptr<LightSampler> create(Renderer& renderer, CommandBuffer& command_buffer) noexcept;

public:
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
};

} // namespace Yutrel