#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>
#include <luisa/runtime/stream.h>

#include "utils/command_buffer.h"

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Renderer;
    class Camera;

    class Integrator
    {
    private:
        const Renderer* m_renderer;

    public:
        explicit Integrator(Renderer& renderer) noexcept;
        ~Integrator() noexcept;

        Integrator() noexcept                    = delete;
        Integrator(const Integrator&)            = delete;
        Integrator& operator=(const Integrator&) = delete;
        Integrator(Integrator&&)                 = delete;
        Integrator& operator=(Integrator&&)      = delete;

    public:
        [[nodiscard]] static luisa::unique_ptr<Integrator> create(Renderer& renderer) noexcept;
        [[nodiscard]] auto& renderer() const noexcept { return m_renderer; }

        void render(Stream& stream);

    private:
        void render_one_frame(CommandBuffer& command_buffer, Camera* camera);
        Float3 Li(const Camera* camera, UInt frame_index, UInt2 pixel_id, Float time) const noexcept;
    };
} // namespace Yutrel