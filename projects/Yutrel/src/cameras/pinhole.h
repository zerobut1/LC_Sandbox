#pragma once

#include <cmath>

#include "base/camera.h"
#include "scene/spec_base.h"

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
        Instance(Renderer& renderer, CommandBuffer& command_buffer, const PinholeCamera* camera, const Film* film, const Filter* filter) noexcept;
        ~Instance() noexcept override = default;

    private:
        [[nodiscard]] Var<Ray> generate_ray_in_camera_space(Expr<float2> pixel, Expr<float> time, Expr<float2> u_lens) const noexcept override;
    };

private:
    float m_fov;

public:
    PinholeCamera(float3 position, float3 lookat, float3 up, float2 shutter_span,
                  uint shutter_samples_count, float fov, bool swaps_handedness) noexcept;
    ~PinholeCamera() noexcept override = default;

public:
    [[nodiscard]] luisa::unique_ptr<Camera::Instance> build(Renderer& renderer, CommandBuffer& command_buffer, const Film* film, const Filter* filter) const noexcept override;
};

class PinholeCameraSpec final : public CameraSpec
{
private:
    float3 _position;
    float3 _lookat;
    float3 _up;
    float2 _shutter_span;
    uint _shutter_samples_count;
    float _fov;
    bool _swaps_handedness;

public:
    PinholeCameraSpec(float3 position, float3 lookat, float3 up, float2 shutter_span,
                      uint shutter_samples_count, float fov,
                      bool swaps_handedness = false) noexcept
        : _position{position}, _lookat{lookat}, _up{up}, _shutter_span{shutter_span},
          _shutter_samples_count{shutter_samples_count}, _fov{fov},
          _swaps_handedness{swaps_handedness} {}

    [[nodiscard]] luisa::optional<luisa::string> validate() const noexcept override
    {
        if (!std::isfinite(_position.x) || !std::isfinite(_position.y) || !std::isfinite(_position.z) ||
            !std::isfinite(_lookat.x) || !std::isfinite(_lookat.y) || !std::isfinite(_lookat.z) ||
            !std::isfinite(_up.x) || !std::isfinite(_up.y) || !std::isfinite(_up.z))
        {
            return spec_validation_error("Camera basis vectors must be finite.");
        }
        auto view = _position - _lookat;
        if (dot(view, view) < 1e-12f || dot(cross(_up, view), cross(_up, view)) < 1e-12f)
        {
            return spec_validation_error("Camera position, look-at and up vectors do not form a valid basis.");
        }
        if (!std::isfinite(_fov) || _fov <= 0.0f || _fov >= 180.0f)
        {
            return spec_validation_error("Pinhole camera FOV must be in (0, 180) degrees.");
        }
        if (!std::isfinite(_shutter_span.x) || !std::isfinite(_shutter_span.y) || _shutter_span.y < _shutter_span.x)
        {
            return spec_validation_error("Camera shutter span is invalid.");
        }
        return luisa::nullopt;
    }
    [[nodiscard]] auto shutter_span() const noexcept { return _shutter_span; }
    [[nodiscard]] const Camera* build(SceneBuilder& builder) const noexcept override;
};

} // namespace Yutrel
