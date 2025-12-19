#pragma once

#include "base/camera.h"

namespace Yutrel
{
    struct PinholeCameraData
    {
        luisa::float2 resolution;
        float tan_half_fov;
    };
} // namespace Yutrel

LUISA_STRUCT(Yutrel::PinholeCameraData, resolution, tan_half_fov){};

namespace Yutrel
{
    class PinholeCamera : public Camera
    {
    public:
        struct CreateInfo
        {
            float fov{45.0f};
        };

    private:
        float m_fov;
        BufferView<PinholeCameraData> m_device_data;

    public:
        explicit PinholeCamera(const CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept;
        ~PinholeCamera() noexcept override = default;

        PinholeCamera() noexcept                       = delete;
        PinholeCamera(const PinholeCamera&)            = delete;
        PinholeCamera& operator=(const PinholeCamera&) = delete;
        PinholeCamera(PinholeCamera&&)                 = delete;
        PinholeCamera& operator=(PinholeCamera&&)      = delete;

    private:
        [[nodiscard]] Sample generate_ray(Expr<uint2> pixel_coord, Expr<float2> u_filter) const noexcept override;
    };

} // namespace Yutrel