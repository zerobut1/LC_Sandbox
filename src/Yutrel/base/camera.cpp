#include "camera.h"

#include "base/film.h"
#include "base/renderer.h"
#include "cameras/pinhole.h"
#include "cameras/thin_lens.h"

#include <numeric>
#include <random>

namespace Yutrel
{
    luisa::unique_ptr<Camera> Camera::create(const CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept
    {
        switch (info.type)
        {
        case Type::pinhole:
            return luisa::make_unique<PinholeCamera>(info, renderer, command_buffer);
        case Type::thin_lens:
            return luisa::make_unique<ThinLensCamera>(info, renderer, command_buffer);
        default:
            return luisa::make_unique<PinholeCamera>(info, renderer, command_buffer);
        }
    }

    Camera::Camera(const CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept
        : m_renderer{&renderer},
          m_film{Film::create(renderer)},
          m_spp{info.spp},
          m_shutter_span{info.shutter_span},
          m_shutter_samples_count{info.shutter_samples_count}
    {
        auto w = normalize(info.position - info.lookat);
        auto u = normalize(cross(info.up, w));
        auto v = cross(w, u);

        m_transform = make_float4x4(make_float4(u, 0.0f),
                                    make_float4(v, 0.0f),
                                    make_float4(w, 0.0f),
                                    make_float4(info.position, 1.0f));

        if (m_shutter_span.y < m_shutter_span.x) [[unlikely]]
        {
            LUISA_ERROR(
                "Invalid time span: [{}, {}]",
                m_shutter_span.x,
                m_shutter_span.y);
        }
        if (m_shutter_span.x != m_shutter_span.y)
        {
            if (m_shutter_samples_count == 0u)
            {
                m_shutter_samples_count = std::min(m_spp, 256u);
            }
            else if (m_shutter_samples_count > m_spp)
            {
                LUISA_WARNING(
                    "Too many shutter samples ({}), "
                    "clamping to samples per pixel ({})",
                    m_shutter_samples_count,
                    m_spp);
                m_shutter_samples_count = m_spp;
            }
        }
    }

    Camera::~Camera() noexcept = default;

    Camera::Sample Camera::generate_ray(Expr<uint2> pixel_coord, Expr<float> time, Expr<float2> u_filter, Expr<float2> u_lens) const noexcept
    {
        auto filter_offset = lerp(-0.5f, 0.5f, u_filter);
        auto pixel         = make_float2(pixel_coord) + 0.5f + filter_offset;

        auto ray_cs = generate_ray_in_camera_space(pixel, time, u_lens);

        auto c2w    = transform();
        auto origin = make_float3(c2w * make_float4(ray_cs->origin(), 1.0f));

        auto d_camera  = make_float3x3(c2w) * ray_cs->direction();
        auto len       = length(d_camera);
        auto direction = ite(len < 1e-7f, make_float3(0.0f, 0.0f, -1.0f), d_camera / len);

        auto ray = make_ray(origin, direction);

        return {std::move(ray), pixel};
    }

    luisa::vector<Camera::ShutterSample> Camera::shutter_samples() const noexcept
    {
        if (m_shutter_span.x == m_shutter_span.y)
        {
            return {ShutterSample{m_shutter_span.x, 1.0f, m_spp}};
        }

        luisa::vector<ShutterSample> buckets(m_shutter_samples_count);
        auto duration = m_shutter_span.y - m_shutter_span.x;
        auto inv_n    = 1.0f / static_cast<float>(m_shutter_samples_count);
        std::uniform_real_distribution<float> dist{};
        std::default_random_engine random{std::random_device{}()};

        for (auto sample = 0u; sample < m_shutter_samples_count; sample++)
        {
            auto ts         = static_cast<float>(sample) * inv_n * duration;
            auto te         = static_cast<float>(sample + 1u) * inv_n * duration;
            auto a          = dist(random);
            auto t          = m_shutter_span.x + std::lerp(ts, te, a);
            auto w          = 1.0f;
            buckets[sample] = ShutterSample{t, w};
        }

        luisa::vector<uint> indices(m_shutter_samples_count);
        std::iota(indices.begin(), indices.end(), 0u);
        std::shuffle(indices.begin(), indices.end(), random);
        auto remainder          = m_spp % m_shutter_samples_count;
        auto samples_per_bucket = m_spp / m_shutter_samples_count;
        for (auto i = 0u; i < remainder; i++)
        {
            buckets[indices[i]].spp = samples_per_bucket + 1u;
        }
        for (auto i = remainder; i < m_shutter_samples_count; i++)
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
            auto scale = static_cast<float>(m_spp) / sum_weights;
            for (auto& s : buckets)
            {
                s.weight = static_cast<float>(s.weight * scale);
            }
        }

        return buckets;
    }

} // namespace Yutrel