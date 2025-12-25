#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>
#include <luisa/runtime/stream.h>

#include "base/camera.h"
#include "utils/command_buffer.h"

#include <rtweekend/rtweekend.h>

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;
    using namespace RTWeekend;

    class Renderer;
    class Sampler;

    class Integrator
    {
    private:
        const Renderer& m_renderer;

        luisa::unique_ptr<Sampler> m_sampler;

        luisa::unique_ptr<HittableList> m_world;
        Polymorphic<Material> m_materials;

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
        [[nodiscard]] auto sampler() const noexcept { return m_sampler.get(); }

        void render(Stream& stream);

    private:
        void render_one_camera(CommandBuffer& command_buffer, Camera::Instance* camera);
        Float3 Li(const Camera::Instance* camera, Expr<uint> frame_index, Expr<uint2> pixel_id, Expr<float> time) const noexcept;

        [[nodiscard]] Float3 ray_color(Var<Ray> ray, Expr<float> time) const noexcept;
    };
} // namespace Yutrel