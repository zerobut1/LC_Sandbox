// Test for Yutrel correctness-mode primitives.
// This test covers firefly limiting and device-side non-finite diagnostics.

#include <array>
#include <cmath>
#include <limits>

#include <luisa/core/logging.h>
#include <luisa/dsl/sugar.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/buffer.h>
#include <luisa/runtime/stream.h>

#include "base/film.h"
#include "base/renderer.h"
#include "utils/frame.h"

using namespace Yutrel;
using namespace luisa;
using namespace luisa::compute;

namespace
{

[[nodiscard]] bool test_firefly_limit(Device& device)
{
    auto stream = device.create_stream();
    auto output = device.create_buffer<float>(2u);

    Kernel1D kernel = [](BufferFloat result) noexcept
    {
        auto normal      = Film::apply_firefly_limit(make_float3(1000.0f, 0.0f, 0.0f), 1.0f, false);
        auto correctness = Film::apply_firefly_limit(make_float3(1000.0f, 0.0f, 0.0f), 1.0f, true);
        result.write(0u, normal.x);
        result.write(1u, correctness.x);
    };
    auto shader = device.compile(kernel);
    std::array<float, 2u> values{};
    stream << shader(output).dispatch(1u)
           << output.copy_to(luisa::span{values.data(), values.size()})
           << synchronize();

    return std::abs(values[0u] - 256.0f) < 1e-4f &&
           std::abs(values[1u] - 1000.0f) < 1e-4f;
}

[[nodiscard]] bool test_axis_aligned_frame(Device& device)
{
    auto stream = device.create_stream();
    auto output = device.create_buffer<float4>(3u);

    Kernel1D kernel = [](BufferFloat4 result) noexcept
    {
        auto n     = make_float3(0.0f, 1.0f, 0.0f);
        auto frame = Frame::make(n);
        result.write(0u, make_float4(frame.s(), length(frame.s())));
        result.write(1u, make_float4(frame.t(), length(frame.t())));
        result.write(2u, make_float4(dot(frame.s(), n),
                                    dot(frame.t(), n),
                                    dot(frame.s(), frame.t()),
                                    0.0f));
    };
    auto shader = device.compile(kernel);
    std::array<float4, 3u> values{};
    stream << shader(output).dispatch(1u)
           << output.copy_to(luisa::span{values.data(), values.size()})
           << synchronize();

    for (auto value : values)
    {
        if (!std::isfinite(value.x) || !std::isfinite(value.y) ||
            !std::isfinite(value.z) || !std::isfinite(value.w))
        {
            return false;
        }
    }
    return std::abs(values[0u].w - 1.0f) < 1e-4f &&
           std::abs(values[1u].w - 1.0f) < 1e-4f &&
           std::abs(values[2u].x) < 1e-4f &&
           std::abs(values[2u].y) < 1e-4f &&
           std::abs(values[2u].z) < 1e-4f;
}

[[nodiscard]] bool test_non_finite_diagnostics(Device& device)
{
    Renderer renderer{device, RendererOptions{.correctness = true}};
    auto stream = device.create_stream();
    auto input  = device.create_buffer<float>(2u);

    Kernel1D kernel = [&](BufferFloat values) noexcept
    {
        auto value   = values.read(dispatch_x());
        auto has_nan = compute::isnan(value);
        auto has_inf = compute::isinf(value);
        renderer.record_path_non_finite(has_nan, has_inf);
        renderer.record_film_non_finite(has_nan, has_inf);
    };
    auto shader = device.compile(kernel);
    std::array<float, 2u> input_values{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
    };
    std::array<uint, 4u> diagnostics{};
    CommandBuffer command_buffer{stream};
    renderer.reset_diagnostics(command_buffer);
    command_buffer
        << input.copy_from(luisa::span{input_values.data(), input_values.size()})
        << shader(input).dispatch(2u);
    renderer.download_diagnostics(command_buffer, diagnostics);
    command_buffer << synchronize();

    return diagnostics[0u] == 1u && diagnostics[1u] == 1u &&
           diagnostics[2u] == 1u && diagnostics[3u] == 1u;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        LUISA_WARNING("Usage: {} <backend>", argv[0]);
        return 1;
    }
    Context context{argv[0]};
    auto device = context.create_device(argv[1]);
    if (!test_firefly_limit(device))
    {
        LUISA_WARNING("Firefly limit correctness test failed.");
        return 2;
    }
    if (!test_axis_aligned_frame(device))
    {
        LUISA_WARNING("Axis-aligned frame test failed.");
        return 3;
    }
    if (!test_non_finite_diagnostics(device))
    {
        LUISA_WARNING("Non-finite diagnostic counter test failed.");
        return 4;
    }
    return 0;
}
