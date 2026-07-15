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
    if (!test_non_finite_diagnostics(device))
    {
        LUISA_WARNING("Non-finite diagnostic counter test failed.");
        return 3;
    }
    return 0;
}
