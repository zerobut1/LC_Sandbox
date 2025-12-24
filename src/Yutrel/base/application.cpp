#include "application.h"

#include <luisa/gui/framerate.h>
#include <luisa/luisa-compute.h>

#include "base/renderer.h"

namespace Yutrel
{
    Application::Application(const CreateInfo& info)
        : m_context(info.bin)
    {
        m_device = m_context.create_device(info.backend);
        m_stream = m_device.create_stream(StreamTag::GRAPHICS);

        m_renderer = Renderer::create(m_device, m_stream);
    }

    Application::~Application() noexcept = default;

    void Application::run()
    {
        m_renderer->render(m_stream);
        m_stream << synchronize();
    }

} // namespace Yutrel
