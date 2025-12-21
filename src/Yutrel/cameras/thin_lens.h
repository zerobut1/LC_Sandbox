#pragma once

#include "base/camera.h"

namespace Yutrel
{
    struct ThinLensCameraData
    {
        float2 pixel_offset;
        float2 resolution;
        float focus_distance{};
        float lens_radius{};
        float projected_pixel_size{};
    };

} // namespace Yutrel

LUISA_STRUCT(Yutrel::ThinLensCameraData,
             pixel_offset, resolution, focus_distance,
             lens_radius, projected_pixel_size){};

namespace Yutrel
{
    class ThinLensCamera : public Camera
    {
    private:
        float m_aperture;
        float m_focal_length;
        float m_focus_distance;
        BufferView<ThinLensCameraData> m_device_data;

    public:
        explicit ThinLensCamera(const Camera::CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept;
        ~ThinLensCamera() noexcept override = default;

        ThinLensCamera() noexcept                        = delete;
        ThinLensCamera(const ThinLensCamera&)            = delete;
        ThinLensCamera& operator=(const ThinLensCamera&) = delete;
        ThinLensCamera(ThinLensCamera&&)                 = delete;
        ThinLensCamera& operator=(ThinLensCamera&&)      = delete;

    private:
        [[nodiscard]] Var<Ray> generate_ray_in_camera_space(Expr<float2> pixel, Expr<float> time, Expr<float2> u_lens) const noexcept override;
        [[nodiscard]] bool requires_lens_sampling() const noexcept override { return true; }

        [[nodiscard]] auto aperture() const noexcept { return m_aperture; }
        [[nodiscard]] auto focal_length() const noexcept { return m_focal_length; }
        [[nodiscard]] auto focus_distance() const noexcept { return m_focus_distance; }
    };

} // namespace Yutrel