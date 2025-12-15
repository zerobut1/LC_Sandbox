#include "film.h"

#include <luisa/luisa-compute.h>

#include "base/renderer.h"

namespace Yutrel
{
    Film::Film(Renderer& renderer) noexcept
        : m_renderer{&renderer}
    {
    }

    Film::~Film() noexcept = default;

    luisa::unique_ptr<Film> Film::create(Renderer& renderer) noexcept
    {
        return luisa::make_unique<Film>(renderer);
    }

    void Film::accumulate(UInt2 pixel_id, Float3 rgb) const noexcept
    {
        LUISA_ASSERT(m_framebuffer, "Film is not prepared.");

        m_framebuffer->write(pixel_id, make_float4(rgb, 1.0f));
    }

    void Film::prepare(CommandBuffer& command_buffer) noexcept
    {
        m_rendering_finished = false;

        auto device = m_renderer->device();
        uint2 size  = m_resolution;

        if (!m_window)
        {
            m_stream = command_buffer.stream();

            m_window    = luisa::make_unique<Window>("Yutrel", size = size);
            m_swapchain = device.create_swapchain(
                *m_stream,
                SwapchainOption{
                    .display           = m_window->native_display(),
                    .window            = m_window->native_handle(),
                    .size              = size,
                    .wants_hdr         = false,
                    .wants_vsync       = false,
                    .back_buffer_count = 2,
                });
            m_framebuffer = device.create_image<float>(m_swapchain.backend_storage(), size);
        }
        m_framerate.clear();
    }

    void Film::release() noexcept
    {
        m_rendering_finished = true;

        CommandBuffer command_buffer{m_stream};

        while (!m_window->should_close())
        {
            this->show(command_buffer);
        }

        command_buffer << synchronize();
        m_window      = nullptr;
        m_framebuffer = {};
    }

    bool Film::show(CommandBuffer& command_buffer) const noexcept
    {
        LUISA_ASSERT(command_buffer.stream() == m_stream, "Command buffer stream mismatch.");

        static const auto target_fps = 60.0;

        if (m_framerate.duration() < 1.0 / target_fps)
        {
            return false;
        }

        if (!m_rendering_finished && m_window->should_close())
        {
            command_buffer << synchronize();
            exit(0);
        }
        command_buffer << commit();

        m_framerate.record();

        m_window->poll_events();

        command_buffer << m_swapchain.present(m_framebuffer);

        // show fps
        auto name = luisa::format("Yutrel - {:.2f} fps", m_framerate.report());
        m_window->set_window_name(name);

        return true;
    }

} // namespace Yutrel