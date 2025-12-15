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

        renderer->m_camera     = Camera::create(*renderer, command_buffer);
        renderer->m_integrator = Integrator::create(*renderer);

        return renderer;
    }

    void Renderer::render(Stream& stream)
    {
        m_integrator->render(stream);
    }

} // namespace Yutrel