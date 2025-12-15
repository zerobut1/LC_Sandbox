#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>

#include "utils/command_buffer.h"

namespace Yutrel
{
    struct CameraData
    {
        luisa::float2 resolution;
        float tan_half_fov;
    };
} // namespace Yutrel

LUISA_STRUCT(Yutrel::CameraData, resolution, tan_half_fov){};

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Renderer;
    class Film;

    struct Sample
    {
        Var<Ray> ray;
        Float2 pixel;
    };

    class Camera
    {
    private:
        const Renderer* m_renderer;
        luisa::unique_ptr<Film> m_film;

        BufferView<CameraData> m_device_data;

        float m_fov;

    public:
        explicit Camera(Renderer& renderer, CommandBuffer& command_buffer) noexcept;
        ~Camera() noexcept;

        Camera() noexcept                = delete;
        Camera(const Camera&)            = delete;
        Camera& operator=(const Camera&) = delete;
        Camera(Camera&&)                 = delete;
        Camera& operator=(Camera&&)      = delete;

    public:
        [[nodiscard]] static luisa::unique_ptr<Camera> create(Renderer& renderer, CommandBuffer& command_buffer) noexcept;

        [[nodiscard]] auto film() const noexcept { return m_film.get(); }
        [[nodiscard]] Sample generate_ray(UInt2 pixel_coord) const noexcept;
    };

} // namespace Yutrel