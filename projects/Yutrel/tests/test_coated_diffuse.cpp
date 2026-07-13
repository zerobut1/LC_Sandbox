#include <array>
#include <cmath>

#include <luisa/luisa-compute.h>

#include "base/renderer.h"
#include "surfaces/coated_diffuse.h"

using namespace luisa;
using namespace luisa::compute;
using namespace Yutrel;

int main(int argc, char* argv[])
{
    if (argc < 2) { return 1; }
    Context context{argv[0]};
    auto device = context.create_device(argv[1]);
    Renderer renderer{device};

    Kernel1D kernel = [&renderer](BufferFloat4 output) noexcept
    {
        SampledWavelengths swl{1u};
        CoatedDiffuse::Closure closure{renderer, swl, 0.0f};

        Interaction it{};
        it.p_g     = make_float3(0.0f);
        it.n_g     = make_float3(0.0f, 0.0f, 1.0f);
        it.shading = Frame{};
        closure.bind(CoatedDiffuse::Closure::Context{
            .it = it,
            .reflectance = SampledSpectrum{1u, 0.5f},
            .alpha = make_float2(0.1f),
            .thickness = 0.01f,
            .medium_albedo = SampledSpectrum{1u},
            .g = 0.0f,
            .eta = 1.5f,
            .max_depth = 10u,
            .samples = 1u,
        });
        closure.pre_eval();
        auto wo = normalize(make_float3(0.2f, 0.1f, 1.0f));
        auto wi = normalize(make_float3(-0.1f, 0.3f, 1.0f));
        auto e  = closure.evaluate(wo, wi);
        auto s  = closure.sample(wo, 0.37f, make_float2(0.23f, 0.71f));
        output.write(0u, make_float4(e.f[0u], e.pdf, s.eval.f[0u], s.eval.pdf));
        closure.post_eval();
    };

    auto shader = device.compile(kernel);
    auto output = device.create_buffer<float4>(1u);
    auto stream = device.create_stream();
    std::array<float4, 1u> result{};
    stream << shader(output).dispatch(1u)
           << output.copy_to(result.data())
           << synchronize();

    auto value = result.front();
    auto valid = std::isfinite(value.x) && std::isfinite(value.y) &&
                 std::isfinite(value.z) && std::isfinite(value.w) &&
                 value.x >= 0.0f && value.y >= 0.0f &&
                 value.z >= 0.0f && value.w >= 0.0f;
    return valid ? 0 : 1;
}
