#include "camera.h"

#include "base/film.h"
#include "base/renderer.h"
#include "cameras/pinhole.h"

namespace Yutrel
{
    luisa::unique_ptr<Camera> Camera::create(const CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept
    {
        switch (info.type)
        {
        case Type::pinhole:
        default:
            return luisa::make_unique<PinholeCamera>(info, renderer, command_buffer);
            break;
        }
    }

    Camera::Camera(const CreateInfo& info, Renderer& renderer, CommandBuffer& command_buffer) noexcept
        : m_renderer{&renderer},
          m_film{Film::create(renderer)},
          m_spp{1024u}
    {
        auto w = normalize(-info.front);
        auto u = normalize(cross(info.up, w));
        auto v = cross(w, u);

        m_transform = make_float4x4(make_float4(u, 0.0f),
                                    make_float4(v, 0.0f),
                                    make_float4(w, 0.0f),
                                    make_float4(info.position, 1.0f));
    }

    Camera::~Camera() noexcept = default;

} // namespace Yutrel