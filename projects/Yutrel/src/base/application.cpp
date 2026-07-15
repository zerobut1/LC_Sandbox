#include "application.h"

#include <luisa/gui/framerate.h>
#include <luisa/luisa-compute.h>

#include "base/renderer.h"
#include "base/scene.h"

namespace Yutrel
{
Application::Application(ApplicationOptions options, const SceneSpec& scene)
    : m_context(options.bin)
{
    m_interactive = options.interactive;
    m_headless    = options.headless;

    m_device = m_context.create_device(options.backend);
    m_stream = m_device.create_stream(StreamTag::GRAPHICS);

    m_scene    = Scene::create(scene);
    m_renderer = Renderer::create(m_device, m_stream, *m_scene,
                                  RendererOptions{.correctness = options.correctness});
}

Application::~Application() noexcept = default;

void Application::run()
{
    if (m_interactive)
    {
        m_renderer->render_interactive(m_stream);
    }
    else
    {
        m_renderer->render(m_stream, !m_headless);
    }
    m_stream << synchronize();
}

} // namespace Yutrel
