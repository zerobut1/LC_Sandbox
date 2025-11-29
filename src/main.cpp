#include <luisa/dsl/sugar.h>
#include <luisa/gui/window.h>
#include <luisa/luisa-compute.h>

using namespace luisa;
using namespace luisa::compute;

int main(int argc, char* argv[])
{
    Context context{argv[1]};
    Device device = context.create_device(argv[2]);

    Kernel2D clear_kernel = [](ImageVar<float> image) noexcept
    {
        Var coord = dispatch_id().xy();
        image.write(coord, make_float4(0.4f, 0.8f, 1.0f, 1.0f));
    };

    Callable plot = [](Float2 uv, Float y) noexcept
    {
        uv.y = 1.0f - uv.y;
        return saturate(smoothstep(y - 0.01f, y, uv.y) - smoothstep(y, y + 0.01f, uv.y));
    };

    Kernel2D main_kernel = [&plot](ImageVar<float> image, Float time, Float2 mouse) noexcept
    {
        Var coord      = dispatch_id().xy();
        Var resolution = make_float2(dispatch_size().xy());
        Var uv         = make_float2(coord) / resolution;
        Var mouse_uv   = mouse / resolution;

        Float x  = uv.x;
        Float3 y = make_float3(x);
        y.x      = sin((x + time) * constants::pi);
        y.y      = smoothstep(0.0f, 1.0f, x);
        y.z      = 1.0f - mouse_uv.y;

        Var colorA = make_float3(0.149f, 0.141f, 0.912f);
        Var colorB = make_float3(1.000f, 0.833f, 0.224f);

        Float3 color = lerp(colorA, colorB, y);
        color        = lerp(color, make_float3(1.0f, 0.0f, 0.0f), plot(uv, y.x));
        color        = lerp(color, make_float3(0.0f, 1.0f, 0.0f), plot(uv, y.y));
        color        = lerp(color, make_float3(0.0f, 0.0f, 1.0f), plot(uv, y.z));

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
    while (!window.should_close())
    {
        window.poll_events();

        auto time = static_cast<float>(clock.toc() * 1e-3);

        stream
            << shader(ldr_image, time, mouse_pos).dispatch(resolution.x, resolution.y)
            << swap_chain.present(ldr_image);
    }
    stream << synchronize();
}