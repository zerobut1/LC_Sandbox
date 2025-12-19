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

    void Film::accumulate(Expr<uint2> pixel, Expr<float3> rgb, Expr<float> effective_spp) const noexcept
    {
        LUISA_ASSERT(m_image, "Film is not prepared.");

        auto pixel_id = pixel.y * resolution().x + pixel.x;
        $if(!any(compute::isnan(rgb) || compute::isinf(rgb)))
        {
            auto threshold = 256.0f * max(effective_spp, 1.f);
            auto abs_rgb   = abs(rgb);
            auto strength  = max(max(max(abs_rgb.x, abs_rgb.y), abs_rgb.z), 0.f);
            auto c         = rgb * (threshold / max(strength, threshold));

            $if(any(c != 0.f))
            {
                m_image->atomic(pixel_id).x.fetch_add(c.x);
                m_image->atomic(pixel_id).y.fetch_add(c.y);
                m_image->atomic(pixel_id).z.fetch_add(c.z);
            };
            $if(effective_spp != 0.f)
            {
                m_image->atomic(pixel_id).w.fetch_add(effective_spp);
            };
        };
    }

    void Film::prepare(CommandBuffer& command_buffer) noexcept
    {
        m_rendering_finished = false;

        auto device      = m_renderer->device();
        uint2 size       = resolution();
        auto pixel_count = size.x * size.y;

        if (!m_image)
        {
            m_image                     = renderer()->device().create_buffer<float4>(pixel_count);
            Kernel1D clear_image_kernel = [](BufferFloat4 image) noexcept
            {
                image.write(dispatch_x(), make_float4(0.f));
            };
            m_clear_image = m_renderer->device().compile(clear_image_kernel);
        }
        command_buffer << m_clear_image(m_image).dispatch(pixel_count);

        if (!m_window)
        {
            m_stream = command_buffer.stream();

            m_window    = luisa::make_unique<Window>("Yutrel", size);
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

            Kernel2D blit_kernel = [&]() noexcept
            {
                auto pixel_coord = dispatch_id().xy();
                auto pixel_id    = pixel_coord.y * resolution().x + pixel_coord.x;
                auto image_data  = m_image->read(pixel_id);
                auto inv_n       = (1.0f / max(image_data.w, 1e-6f));
                auto color       = image_data.xyz() * inv_n;

                // linear to sRGB
                color = ite(color <= .0031308f, color * 12.92f, 1.055f * pow(color, 1.f / 2.4f) - .055f);

                m_framebuffer->write(pixel_coord, make_float4(color, 1.0f));
            };
            m_blit = device.compile(blit_kernel);

            Kernel2D clear_kernel = [](ImageFloat image) noexcept
            {
                image->write(dispatch_id().xy(), make_float4(0.0f));
            };
            m_clear = device.compile(clear_kernel);
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

        m_framerate.record();
        {
            m_window->poll_events();

            command_buffer
                << m_clear(m_framebuffer).dispatch(resolution())
                << m_blit().dispatch(resolution())
                << m_swapchain.present(m_framebuffer)
                << commit();
        }
        auto name = luisa::format("Yutrel - {:.2f} fps", m_framerate.report());
        m_window->set_window_name(name);

        return true;
    }

} // namespace Yutrel