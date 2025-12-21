#pragma once

#include "base/camera.h"

namespace Yutrel
{
    struct PinholeCameraData
    {
        luisa::float2 resolution;
        float tan_half_fov{};
    };
} // namespace Yutrel

LUISA_STRUCT(Yutrel::PinholeCameraData, resolution, tan_half_fov){};

namespace Yutrel
{
    class PinholeCamera : public Camera
    {
    private:
        float m_fov;
        BufferView<PinholeCameraData> m_device_data;

    public:
        explicit PinholeCamera(const Camera::CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept;
        ~PinholeCamera() noexcept override = default;

        PinholeCamera() noexcept                       = delete;
        PinholeCamera(const PinholeCamera&)            = delete;
        PinholeCamera& operator=(const PinholeCamera&) = delete;
        PinholeCamera(PinholeCamera&&)                 = delete;
        PinholeCamera& operator=(PinholeCamera&&)      = delete;

    private:
        [[nodiscard]] Var<Ray> generate_ray_in_camera_space(Expr<float2> pixel, Expr<float> time, Expr<float2> u_lens) const noexcept override;
    };

} // namespace Yutrel