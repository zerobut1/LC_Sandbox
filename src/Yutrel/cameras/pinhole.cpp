#include "pinhole.h"

#include "base/film.h"
#include "base/renderer.h"

namespace Yutrel
{
    PinholeCamera::PinholeCamera(const Camera::CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept
        : Camera(info, renderer, command_buffer),
          m_device_data{renderer.arena_buffer<PinholeCameraData>(1u)},
          m_fov(radians(info.fov))
    {
        PinholeCameraData host_data{make_float2(film()->resolution()), tan(m_fov * 0.5f)};
        m_device_data = renderer.arena_buffer<PinholeCameraData>(1u);
        command_buffer
            << m_device_data.copy_from(&host_data)
            << commit();
    }

    Camera::Sample PinholeCamera::generate_ray(Expr<uint2> pixel_coord, Expr<float2> u_filter) const noexcept
    {
        auto filter_offset = lerp(-0.5f, 0.5f, u_filter);
        auto pixel         = make_float2(pixel_coord) + 0.5f + filter_offset;

        auto data         = m_device_data->read(0u);
        auto p            = (pixel * 2.0f - data.resolution) * (data.tan_half_fov / data.resolution.y);
        auto direction_cs = normalize(make_float3(p.x, -p.y, -1.0f));
        auto origin_cs    = make_float3(0.0f);

        auto c2w = transform();
        auto origin = make_float3(c2w * make_float4(origin_cs, 1.0f));

        auto d_camera  = make_float3x3(c2w) * direction_cs;
        auto len       = length(d_camera);
        auto direction = ite(len < 1e-7f, make_float3(0.0f, 0.0f, -1.0f), d_camera / len);

        auto ray = make_ray(origin, direction);

        return {std::move(ray), pixel};
    }
} // namespace Yutrel