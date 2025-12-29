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
    class PinholeCamera final : public Camera
    {
        class Instance final : public Camera::Instance
        {
        private:
            BufferView<PinholeCameraData> m_device_data;

        public:
            explicit Instance(Renderer& renderer, CommandBuffer& command_buffer, const PinholeCamera* camera) noexcept;
            ~Instance() noexcept override = default;

        private:
            [[nodiscard]] Var<Ray> generate_ray_in_camera_space(Expr<float2> pixel, Expr<float> time, Expr<float2> u_lens) const noexcept override;
        };

    private:
        float m_fov;

    public:
        explicit PinholeCamera(const Camera::CreateInfo& info) noexcept;
        ~PinholeCamera() noexcept override = default;

    public:
        [[nodiscard]] luisa::unique_ptr<Camera::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
    };

} // namespace Yutrel