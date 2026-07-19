#include "camera.h"

#include "base/film.h"
#include "base/renderer.h"
#include <numeric>
#include <random>

namespace Yutrel
{
Camera::Camera(float3 position, float3 lookat, float3 up, float2 shutter_span,
               uint shutter_samples_count, bool swaps_handedness) noexcept
    : m_up{up},
      m_shutter_span{shutter_span},
      m_shutter_samples_count{shutter_samples_count}
{
    auto w = normalize(position - lookat);
    auto u = swaps_handedness ? normalize(cross(up, w)) : normalize(cross(w, up));
    auto v = swaps_handedness ? cross(w, u) : cross(u, w);

    m_init_transform = make_float4x4(make_float4(u, 0.0f),
                                     make_float4(v, 0.0f),
                                     make_float4(w, 0.0f),
                                     make_float4(position, 1.0f));

    if (m_shutter_span.y < m_shutter_span.x) [[unlikely]]
    {
        LUISA_ERROR(
            "Invalid time span: [{}, {}]",
            m_shutter_span.x,
            m_shutter_span.y);
    }
}

Camera::~Camera() noexcept = default;

luisa::vector<Camera::ShutterSample> Camera::shutter_samples(uint spp, uint seed) const noexcept
{
    if (m_shutter_span.x == m_shutter_span.y)
    {
        return {ShutterSample{m_shutter_span.x, 1.0f, spp}};
    }

    auto shutter_samples_count = m_shutter_samples_count == 0u ? std::min(spp, 256u) : m_shutter_samples_count;
    if (shutter_samples_count > spp)
    {
        LUISA_WARNING("Too many shutter samples ({}), clamping to samples per pixel ({}).", shutter_samples_count, spp);
        shutter_samples_count = spp;
    }
    luisa::vector<ShutterSample> buckets(shutter_samples_count);
    auto duration = m_shutter_span.y - m_shutter_span.x;
    auto inv_n    = 1.0f / static_cast<float>(shutter_samples_count);
    std::uniform_real_distribution<float> dist{};
    std::default_random_engine random{seed};

    for (auto sample = 0u; sample < shutter_samples_count; sample++)
    {
        auto ts         = static_cast<float>(sample) * inv_n * duration;
        auto te         = static_cast<float>(sample + 1u) * inv_n * duration;
        auto a          = dist(random);
        auto t          = m_shutter_span.x + std::lerp(ts, te, a);
        auto w          = 1.0f;
        buckets[sample] = ShutterSample{t, w};
    }

    luisa::vector<uint> indices(shutter_samples_count);
    std::iota(indices.begin(), indices.end(), 0u);
    std::shuffle(indices.begin(), indices.end(), random);
    auto remainder          = spp % shutter_samples_count;
    auto samples_per_bucket = spp / shutter_samples_count;
    for (auto i = 0u; i < remainder; i++)
    {
        buckets[indices[i]].spp = samples_per_bucket + 1u;
    }
    for (auto i = remainder; i < shutter_samples_count; i++)
    {
        buckets[indices[i]].spp = samples_per_bucket;
    }
    auto sum_weights = std::accumulate(
        buckets.cbegin(),
        buckets.cend(),
        0.0f,
        [](float acc, const ShutterSample& s)
    {
        return acc + s.weight * s.spp;
    });

    if (sum_weights == 0.0) [[unlikely]]
    {
        LUISA_WARNING_WITH_LOCATION(
            "Invalid shutter samples generated. "
            "Falling back to uniform shutter curve.");
        for (auto& s : buckets)
        {
            s.weight = 1.0f;
        }
    }
    else
    {
        auto scale = static_cast<float>(spp) / sum_weights;
        for (auto& s : buckets)
        {
            s.weight = static_cast<float>(s.weight * scale);
        }
    }

    return buckets;
}

Camera::Instance::Instance(Renderer& renderer, CommandBuffer& command_buffer, const Camera* camera, const Film* film, const Filter* filter) noexcept
    : m_renderer(renderer),
      m_camera(camera),
      m_film(film->build(renderer, command_buffer)),
      m_filter(filter->build(renderer)),
      m_host_transform(camera->init_transform()),
      m_device_transform(renderer.arena_buffer<float4x4>(1u))
{
    command_buffer
        << m_device_transform.copy_from(&m_host_transform)
        << commit();
}

void Camera::Instance::set_transform(CommandBuffer& command_buffer, const float4x4& c2w) noexcept
{
    m_host_transform = c2w;
    command_buffer
        << m_device_transform.copy_from(&c2w)
        << commit();
}

Camera::Sample Camera::Instance::generate_ray(Expr<uint2> pixel_coord, Expr<float> time, Expr<float2> u_filter, Expr<float2> u_lens) const noexcept
{
    auto [filter_offset, filter_weight] = m_filter->sample(u_filter);

    auto pixel = make_float2(pixel_coord) + 0.5f + filter_offset;

    auto ray_cs = generate_ray_in_camera_space(pixel, time, u_lens);

    auto c2w    = m_device_transform->read(0u);
    auto origin = make_float3(c2w * make_float4(ray_cs->origin(), 1.0f));

    auto d_camera  = make_float3x3(c2w) * ray_cs->direction();
    auto len       = length(d_camera);
    auto direction = ite(len < 1e-7f, make_float3(0.0f, 0.0f, -1.0f), d_camera / len);

    auto ray = make_ray(origin, direction);

    return {std::move(ray), pixel, filter_weight};
}

} // namespace Yutrel
