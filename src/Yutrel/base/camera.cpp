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
            PinholeCamera::CreateInfo pinhole_info{
                .fov = info.fov,
            };

            return luisa::make_unique<PinholeCamera>(pinhole_info, renderer, command_buffer);
            break;
        }
    }

    Camera::Camera(Renderer& renderer, CommandBuffer& command_buffer) noexcept
        : m_renderer{&renderer},
          m_film{Film::create(renderer)},
          m_spp{1024u}
    {
    }

    Camera::~Camera() noexcept = default;

    

} // namespace Yutrel