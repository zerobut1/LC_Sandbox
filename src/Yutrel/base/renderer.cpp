#include "renderer.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/integrator.h"

namespace Yutrel
{

    Renderer::Renderer(Device& device) noexcept
        : m_device(device)
    {
    }

    Renderer::~Renderer() noexcept = default;

    luisa::unique_ptr<Renderer> Renderer::create(Device& device, Stream& stream) noexcept
    {
        auto renderer = luisa::make_unique<Renderer>(device);

        CommandBuffer command_buffer{&stream};

        // TODO: from scene description
        Camera::CreateInfo camera_info{
            .type           = Camera::Type::thin_lens,
            .spp            = 1024u,
            .position       = make_float3(13.0f, 2.0f, 3.0f),
            .lookat         = make_float3(0.0f, 0.0f, 0.0f),
            .up             = make_float3(0.0f, 1.0f, 0.0f),
            .aperture       = 0.4f,
            .focal_length   = 78.0f,
            .focus_distance = 13.0f,
        };

        renderer->m_camera     = Camera::create(camera_info, *renderer, command_buffer);
        renderer->m_integrator = Integrator::create(*renderer);

        command_buffer << synchronize();

        return renderer;
    }

    void Renderer::render(Stream& stream)
    {
        m_integrator->render(stream);
    }

} // namespace Yutrel