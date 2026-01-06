#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>
#include <luisa/runtime/stream.h>

#include "base/camera.h"
#include "utils/command_buffer.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Renderer;
class Sampler;
class LightSampler;

class Integrator
{
public:
    [[nodiscard]] static luisa::unique_ptr<Integrator> create(Renderer& renderer, CommandBuffer& command_buffer) noexcept;

private:
    const Renderer& m_renderer;

    uint m_max_depth{10u};
    uint m_rr_depth{0u};
    float m_rr_threshold{0.05f};

    luisa::unique_ptr<Sampler> m_sampler;
    luisa::unique_ptr<LightSampler> m_light_sampler;

public:
    explicit Integrator(Renderer& renderer, CommandBuffer& command_buffer) noexcept;
    ~Integrator() noexcept;

    Integrator() noexcept                    = delete;
    Integrator(const Integrator&)            = delete;
    Integrator& operator=(const Integrator&) = delete;
    Integrator(Integrator&&)                 = delete;
    Integrator& operator=(Integrator&&)      = delete;

public:
    [[nodiscard]] auto& renderer() const noexcept { return m_renderer; }
    [[nodiscard]] auto max_depth() const noexcept { return m_max_depth; }
    [[nodiscard]] auto rr_depth() const noexcept { return m_rr_depth; }
    [[nodiscard]] auto rr_threshold() const noexcept { return m_rr_threshold; }
    [[nodiscard]] auto sampler() const noexcept { return m_sampler.get(); }
    [[nodiscard]] auto light_sampler() const noexcept { return m_light_sampler.get(); }

    void render(Stream& stream);
    void render_interactive(Stream& stream);

private:
    void render_one_camera(CommandBuffer& command_buffer, Camera::Instance* camera);
    Float3 Li(const Camera::Instance* camera, Expr<uint> frame_index, Expr<uint2> pixel_id, Expr<float> time) const noexcept;
};
} // namespace Yutrel