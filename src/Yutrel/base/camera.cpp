#include "camera.h"

#include "base/film.h"
#include "base/renderer.h"
#include "cameras/pinhole.h"
#include "cameras/thin_lens.h"

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
          m_spp{1024u}
    {
        auto w = normalize(info.position - info.lookat);
        auto u = normalize(cross(info.up, w));
        auto v = cross(w, u);

        m_transform = make_float4x4(make_float4(u, 0.0f),
                                    make_float4(v, 0.0f),
                                    make_float4(w, 0.0f),
                                    make_float4(info.position, 1.0f));
    }

    Camera::~Camera() noexcept = default;

    Camera::Sample Camera::generate_ray(Expr<uint2> pixel_coord, Expr<float2> u_filter, Expr<float2> u_lens) const noexcept
    {
        auto filter_offset = lerp(-0.5f, 0.5f, u_filter);
        auto pixel         = make_float2(pixel_coord) + 0.5f + filter_offset;

        auto ray_cs = generate_ray_in_camera_space(pixel, u_lens);

        auto c2w    = transform();
        auto origin = make_float3(c2w * make_float4(ray_cs->origin(), 1.0f));

        auto d_camera  = make_float3x3(c2w) * ray_cs->direction();
        auto len       = length(d_camera);
        auto direction = ite(len < 1e-7f, make_float3(0.0f, 0.0f, -1.0f), d_camera / len);

        auto ray = make_ray(origin, direction);

        return {std::move(ray), pixel};
    }

} // namespace Yutrel