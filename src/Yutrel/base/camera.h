#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>

#include "utils/command_buffer.h"

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Renderer;
    class Film;

    class Camera
    {
    public:
        enum class Type
        {
            pinhole,
            thin_lens,
        };

        struct CreateInfo
        {
            Type type{Type::pinhole};
            float3 position{};
            float3 lookat{};
            float3 up{0.0f, 1.0f, 0.0f};

            // pinhole
            float fov{45.0f};

            // thin lens
            float aperture{2.0f};
            float focal_length{35.0f};
        };

        [[nodiscard]] static luisa::unique_ptr<Camera> create(const CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept;

        struct Sample
        {
            Var<Ray> ray;
            Float2 pixel;
        };

    private:
        const Renderer* m_renderer;
        luisa::unique_ptr<Film> m_film;

        float4x4 m_transform;
        uint m_spp;

    public:
        explicit Camera(const CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept;
        virtual ~Camera() noexcept;

        Camera() noexcept                = delete;
        Camera(const Camera&)            = delete;
        Camera& operator=(const Camera&) = delete;
        Camera(Camera&&)                 = delete;
        Camera& operator=(Camera&&)      = delete;

    public:
        [[nodiscard]] auto film() const noexcept { return m_film.get(); }
        [[nodiscard]] auto spp() const noexcept { return m_spp; }
        [[nodiscard]] auto transform() const noexcept { return m_transform; }

        [[nodiscard]] Sample generate_ray(Expr<uint2> pixel_coord, Expr<float2> u_filter, Expr<float2> u_lens) const noexcept;
        [[nodiscard]] virtual Var<Ray> generate_ray_in_camera_space(Expr<float2> pixel, Expr<float2> u_lens) const noexcept = 0;
        [[nodiscard]] virtual bool requires_lens_sampling() const noexcept { return false; }
    };

} // namespace Yutrel