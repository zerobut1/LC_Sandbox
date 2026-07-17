#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include <luisa/luisa-compute.h>

#include "base/renderer.h"
#include "surfaces/coated_diffuse.h"
#include "utils/scattering.h"

using namespace luisa;
using namespace luisa::compute;
using namespace Yutrel;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        return 1;
    }
    Context context{argv[0]};
    auto device = context.create_device(argv[1]);
    Renderer renderer{device};

    Kernel1D kernel = [&renderer](BufferFloat4 output) noexcept
    {
        SampledWavelengths swl{1u};
        Interaction it{};
        it.p_g     = make_float3(0.0f);
        it.n_g     = make_float3(0.0f, 0.0f, 1.0f);
        it.shading = Frame{};

        CoatedDiffuse::Closure closure{renderer, swl, 0.0f};
        closure.bind(CoatedDiffuse::Closure::Context{
            .it            = it,
            .reflectance   = SampledSpectrum{1u, 0.5f},
            .alpha         = make_float2(0.1f),
            .thickness     = 0.01f,
            .medium_albedo = SampledSpectrum{1u},
            .g             = 0.0f,
            .eta           = 1.5f,
            .max_depth     = 10u,
            .samples       = 1u,
        });
        closure.pre_eval();
        auto wo = normalize(make_float3(0.2f, 0.1f, 1.0f));
        auto wi = normalize(make_float3(-0.1f, 0.3f, 1.0f));
        auto e  = closure.evaluate(wo, wi);
        auto s  = closure.sample(wo, 0.37f, make_float2(0.23f, 0.71f));
        output.write(0u, make_float4(e.f[0u], e.pdf, s.eval.f[0u], s.eval.pdf));
        closure.post_eval();

        CoatedDiffuse::Closure analytic{renderer, swl, 0.0f};
        analytic.bind(CoatedDiffuse::Closure::Context{
            .it            = it,
            .reflectance   = SampledSpectrum{1u, 0.5f},
            .alpha         = make_float2(0.1f),
            .thickness     = 0.01f,
            .medium_albedo = SampledSpectrum{1u},
            .g             = 0.0f,
            .eta           = 1.0f,
            .max_depth     = 1u,
            .samples       = 1u,
        });
        analytic.pre_eval();
        auto ae           = analytic.evaluate(wo, wi);
        auto expected_f   = 0.5f * inv_pi * wi.z *
                            exp(-0.01f / wo.z) * exp(-0.01f / wi.z);
        auto expected_pdf = 0.1f * 0.25f * inv_pi + 0.9f * wi.z * inv_pi;
        output.write(1u, make_float4(ae.f[0u], expected_f, ae.pdf, expected_pdf));
        analytic.post_eval();

        CoatedDiffuse::Closure delta{renderer, swl, 0.0f};
        delta.bind(CoatedDiffuse::Closure::Context{
            .it            = it,
            .reflectance   = SampledSpectrum{1u, 0.5f},
            .alpha         = make_float2(0.0005f),
            .thickness     = 0.01f,
            .medium_albedo = SampledSpectrum{1u},
            .g             = 0.0f,
            .eta           = 1.5f,
            .max_depth     = 1u,
            .samples       = 1u,
        });
        delta.pre_eval();
        auto ds = delta.sample(wo, 0.0f, make_float2(0.5f));
        output.write(2u, make_float4(ds.eval.pdf, ds.pdf_mis, ite(ds.delta, 1.0f, 0.0f), TrowbridgeReitzDistribution::roughness_to_alpha(0.25f)));
        delta.post_eval();
    };

    auto shader = device.compile(kernel);
    auto output = device.create_buffer<float4>(3u);
    auto stream = device.create_stream();
    std::array<float4, 3u> result{};
    stream << shader(output).dispatch(1u)
           << output.copy_to(luisa::span{result})
           << synchronize();

    auto value              = result[0u];
    auto finite_nonnegative = [](float4 v) noexcept
    {
        return std::isfinite(v.x) && std::isfinite(v.y) &&
               std::isfinite(v.z) && std::isfinite(v.w) &&
               v.x >= 0.0f && v.y >= 0.0f && v.z >= 0.0f && v.w >= 0.0f;
    };
    auto relative_error = [](float a, float b) noexcept
    {
        return std::abs(a - b) / std::max(1e-6f, std::abs(b));
    };
    auto analytic_value = result[1u];
    auto delta_value    = result[2u];
    auto valid          = finite_nonnegative(value) && finite_nonnegative(analytic_value) &&
                          finite_nonnegative(delta_value) &&
                          relative_error(analytic_value.x, analytic_value.y) < 1e-4f &&
                          relative_error(analytic_value.z, analytic_value.w) < 1e-4f &&
                          delta_value.x > 0.0f && delta_value.y >= 0.0f &&
                          delta_value.z == 1.0f && relative_error(delta_value.w, 0.5f) < 1e-6f;
    if (!valid)
    {
        std::fprintf(stderr,
                     "rough=(%g,%g,%g,%g) analytic=(%g,%g,%g,%g) delta=(%g,%g,%g,%g)\n",
                     value.x,
                     value.y,
                     value.z,
                     value.w,
                     analytic_value.x,
                     analytic_value.y,
                     analytic_value.z,
                     analytic_value.w,
                     delta_value.x,
                     delta_value.y,
                     delta_value.z,
                     delta_value.w);
    }
    return valid ? 0 : 1;
}
