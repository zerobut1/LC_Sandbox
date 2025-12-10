#include "application.h"

#include <luisa/gui/framerate.h>
#include <luisa/luisa-compute.h>

namespace Yutrel
{
    Application::Application(const ApplicationCreateInfo& info)
        : m_context{info.bin},
          m_window_name(luisa::format("{} - {}", info.window_name, info.backend)),
          m_resolution(info.resolution),
          m_window(m_window_name, m_resolution)
    {
        m_device = m_context.create_device(info.backend);
        m_stream = m_device.create_stream(StreamTag::GRAPHICS);

        m_swapchain = m_device.create_swapchain(
            m_stream,
            SwapchainOption{
                .display           = m_window.native_display(),
                .window            = m_window.native_handle(),
                .size              = m_resolution,
                .wants_hdr         = false,
                .wants_vsync       = false,
                .back_buffer_count = 2,
            });
    }

    void Application::run()
    {
        // Create and clear device image
        Kernel2D clear_kernel = [](ImageVar<float> image) noexcept
        {
            Var coord = dispatch_id().xy();
            image.write(coord, make_float4(0.4f, 0.8f, 1.0f, 1.0f));
        };
        auto clear = m_device.compile(clear_kernel);

        Image<float> device_image = m_device.create_image<float>(m_swapchain.backend_storage(), m_resolution);

        m_stream << clear(device_image).dispatch(m_resolution.x, m_resolution.y);

        // temp
        Kernel2D main_kernel = [](ImageVar<float> image, Float time) noexcept
        {
            Var coord      = dispatch_id().xy();
            Var resolution = make_float2(dispatch_size().xy());
            Var uv         = make_float2(coord) / resolution;

            Float3 color = make_float3(uv, 0.0f);

            image.write(coord, make_float4(color, 1.0f));
        };

        auto main_shader = m_device.compile(main_kernel);

        // Main Loop
        Clock clock;
        Framerate framerate;
        size_t frame_count = 0;
        while (!m_window.should_close())
        {
            m_window.poll_events();

            auto time = static_cast<float>(clock.toc() * 1e-3);

            m_stream
                << main_shader(device_image, time).dispatch(m_resolution.x, m_resolution.y)
                << m_swapchain.present(device_image);

            frame_count++;
            if (framerate.duration() > 0.5)
            {
                framerate.record(frame_count);
                frame_count = 0;
                auto name   = luisa::format("{} - {:.2f} fps", m_window_name, framerate.report());
                m_window.set_window_name(name);
            }
        }
        m_stream << synchronize();
    }

} // namespace Yutrel
