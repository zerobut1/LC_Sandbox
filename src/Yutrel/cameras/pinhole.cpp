#include "pinhole.h"

#include "base/film.h"
#include "base/renderer.h"

namespace Yutrel
{
    PinholeCamera::PinholeCamera(const Camera::CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept
        : Camera(info, renderer, command_buffer),
          m_device_data(renderer.arena_buffer<PinholeCameraData>(1u)),
          m_fov(radians(info.fov))
    {
        PinholeCameraData host_data{make_float2(film()->resolution()), tan(m_fov * 0.5f)};
        m_device_data = renderer.arena_buffer<PinholeCameraData>(1u);
        command_buffer
            << m_device_data.copy_from(&host_data)
            << commit();
    }

    Var<Ray> PinholeCamera::generate_ray_in_camera_space(Expr<float2> pixel, Expr<float> time, Expr<float2> u_lens) const noexcept
    {
        auto data         = m_device_data->read(0u);
        auto p            = (pixel * 2.0f - data.resolution) * (data.tan_half_fov / data.resolution.y);
        auto direction_cs = normalize(make_float3(p.x, -p.y, -1.0f));

        return make_ray(make_float3(0.0f), direction_cs);
    }
} // namespace Yutrel