#include "thin_lens.h"

#include "base/film.h"
#include "base/renderer.h"
#include "utils/sampling.h"

namespace Yutrel
{
ThinLensCamera::ThinLensCamera(Scene& scene, const Camera::CreateInfo& info) noexcept
    : Camera(scene, info),
      m_aperture(info.aperture),
      m_focal_length(info.focal_length),
      m_focus_distance(info.focus_distance)
{
    m_focus_distance = max(m_focus_distance, 1e-4f);
}

luisa::unique_ptr<Camera::Instance> ThinLensCamera::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    return luisa::make_unique<Instance>(renderer, command_buffer, this);
}

ThinLensCamera::Instance::Instance(Renderer& renderer, CommandBuffer& command_buffer, const ThinLensCamera* camera) noexcept
    : Camera::Instance(renderer, command_buffer, camera),
      m_device_data(renderer.arena_buffer<ThinLensCameraData>(1u))
{
    auto v                      = camera->focus_distance();
    auto f                      = camera->focal_length() * 1e-3;
    auto u                      = 1.0 / (1.0 / f - 1.0 / v);
    auto object_to_sensor_ratio = static_cast<float>(v / u);
    auto lens_radius            = static_cast<float>(.5 * f / camera->aperture());
    auto resolution             = make_float2(film()->base()->resolution());
    auto pixel_offset           = 0.5f * resolution;
    auto projected_pixel_size =
        resolution.x > resolution.y
            ? min(static_cast<float>(object_to_sensor_ratio * .036 / resolution.x),
                  static_cast<float>(object_to_sensor_ratio * .024 / resolution.y))
            : min(static_cast<float>(object_to_sensor_ratio * .024 / resolution.x),
                  static_cast<float>(object_to_sensor_ratio * .036 / resolution.y));

    ThinLensCameraData host_data{
        .pixel_offset         = pixel_offset,
        .resolution           = resolution,
        .focus_distance       = v,
        .lens_radius          = lens_radius,
        .projected_pixel_size = projected_pixel_size,
    };
    command_buffer
        << m_device_data.copy_from(&host_data)
        << commit();
}

[[nodiscard]] Var<Ray> ThinLensCamera::Instance::generate_ray_in_camera_space(Expr<float2> pixel, Expr<float> time, Expr<float2> u_lens) const noexcept
{
    auto data        = m_device_data->read(0u);
    auto coord_focal = (pixel - data.pixel_offset) * data.projected_pixel_size;
    auto p_focal     = make_float3(coord_focal.x, -coord_focal.y, -data.focus_distance);
    auto coord_lens  = sample_uniform_disk_concentric(u_lens) * data.lens_radius;
    auto p_lens      = make_float3(coord_lens, 0.0f);
    return make_ray(p_lens, normalize(p_focal - p_lens));
}

} // namespace Yutrel