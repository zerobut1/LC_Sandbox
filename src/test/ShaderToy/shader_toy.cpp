#include <luisa/dsl/sugar.h>
#include <luisa/gui/framerate.h>
#include <luisa/gui/window.h>
#include <luisa/luisa-compute.h>

#include "utils.h"

using namespace luisa;
using namespace luisa::compute;
using namespace Utils;

int main(int argc, char* argv[])
{
    Context context{argv[0]};

    if (argc <= 1)
    {
        LUISA_INFO("Usage: {} <backend>. <backend>: cuda, dx, vk", argv[0]);
        exit(1);
    }
    Device device = context.create_device(argv[1]);

    Kernel2D clear_kernel = [](ImageVar<float> image) noexcept
    {
        Var coord = dispatch_id().xy();
        image.write(coord, make_float4(0.4f, 0.8f, 1.0f, 1.0f));
    };

    Kernel2D main_kernel = [](ImageVar<float> image, Float time, Float2 mouse) noexcept
    {
        Var coord      = dispatch_id().xy();
        Var resolution = make_float2(dispatch_size().xy());
        Var uv         = make_float2(coord) / resolution;
        Var mouse_uv   = mouse / resolution;
        Var st         = uv;
        st.x           = st.x * resolution.x / resolution.y;
        Float3 color   = make_float3(0.0f);

        st *= 10.0f;

        Float v         = 0.0f;
        Float amplitude = 0.5f;
        Float frequency = 0.0f;

        const UInt OCTAVES = 8u;

        $for(i, OCTAVES)
        {
            v += amplitude * noise(st);
            st *= 2.0f;
            amplitude *= 0.5f;
        };

        color = make_float3(v);

        image.write(coord, make_float4(color, 1.0f));
    };

    auto clear  = device.compile(clear_kernel);
    auto shader = device.compile(main_kernel);

    static constexpr uint2 resolution{1024u, 1024u};
    Stream stream = device.create_stream(StreamTag::GRAPHICS);

    Window window{"ShaderToy", resolution};

    Swapchain swap_chain = device.create_swapchain(
        stream,
        SwapchainOption{
            .display           = window.native_display(),
            .window            = window.native_handle(),
            .size              = resolution,
            .wants_hdr         = false,
            .wants_vsync       = false,
            .back_buffer_count = 2,
        });
    Image<float> ldr_image = device.create_image<float>(swap_chain.backend_storage(), resolution);

    float2 mouse_pos{};
    window.set_cursor_position_callback([&](float2 cursor_pos)
    {
        mouse_pos = cursor_pos;
    });

    stream << clear(ldr_image).dispatch(resolution.x, resolution.y);

    Clock clock;
    Framerate framerate;
    size_t frame_count = 0;
    while (!window.should_close())
    {
        window.poll_events();

        auto time = static_cast<float>(clock.toc() * 1e-3);

        stream
            << shader(ldr_image, time, mouse_pos).dispatch(resolution.x, resolution.y)
            << swap_chain.present(ldr_image);

        frame_count++;
        if (framerate.duration() > 0.5)
        {
            framerate.record(frame_count);
            frame_count = 0;
        }
    }
    stream << synchronize();
}