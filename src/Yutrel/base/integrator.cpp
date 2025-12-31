#include "integrator.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/film.h"
#include "base/geometry.h"
#include "base/interaction.h"
#include "base/renderer.h"
#include "base/sampler.h"
#include "utils/command_buffer.h"
#include "utils/progress_bar.h"

namespace Yutrel
{
Integrator::Integrator(Renderer& renderer) noexcept
    : m_renderer(renderer),
      m_sampler(Sampler::create(renderer)) {}

Integrator::~Integrator() noexcept = default;

luisa::unique_ptr<Integrator> Integrator::create(Renderer& renderer) noexcept
{
    return luisa::make_unique<Integrator>(renderer);
}

void Integrator::render(Stream& stream)
{
    CommandBuffer command_buffer{stream};

    auto camera = m_renderer.camera();

    camera->film()->prepare(command_buffer);
    {
        render_one_camera(command_buffer, camera);
    }
    camera->film()->release();
}

void Integrator::render_one_camera(CommandBuffer& command_buffer, Camera::Instance* camera)
{
    auto spp        = camera->base()->spp();
    auto resolution = camera->film()->base()->resolution();

    sampler()->reset(command_buffer, resolution.x * resolution.y);
    command_buffer << synchronize();

    LUISA_INFO(
        "Rendering of resolution {}x{} at {}spp.",
        resolution.x,
        resolution.y,
        spp);

    Kernel2D render_kernel = [&](UInt frame_index, Float time, Float shutter_weight) noexcept
    {
        set_block_size(16u, 16u, 1u);
        Var pixel_id = dispatch_id().xy();
        Var L        = Li(camera, frame_index, pixel_id, time);
        camera->film()->accumulate(pixel_id, L * shutter_weight, 1.0f);
    };

    LUISA_INFO("Start compiling Integrator shader");
    Clock clock_compile;
    auto render = renderer().device().compile(render_kernel);
    LUISA_INFO("Integrator shader compile in {} ms.", clock_compile.toc());
    command_buffer << synchronize();

    auto shutter_samples = camera->base()->shutter_samples();
    LUISA_INFO("Rendering started.");
    Clock clock_render;
    ProgressBar progress_bar;
    progress_bar.update(0.0);
    auto dispatch_count      = 0u;
    auto global_sample_index = 0u;
    for (const auto& s : shutter_samples)
    {
        for (auto i = 0u; i < s.spp; i++)
        {
            dispatch_count++;
            command_buffer << render(global_sample_index++, s.time, s.weight).dispatch(resolution);
            const auto dispatches_per_commit = 4u;
            if (camera->film()->show(command_buffer) || dispatch_count >= dispatches_per_commit) [[unlikely]]
            {
                dispatch_count = 0u;
                auto p         = global_sample_index / static_cast<double>(spp);
                command_buffer << [&progress_bar, p]
                {
                    progress_bar.update(p);
                };
            }
        }
    }
    command_buffer << synchronize();
    progress_bar.done();
    LUISA_INFO("Rendering finished in {} ms.", clock_render.toc());
}

Float3 Integrator::Li(const Camera::Instance* camera, Expr<uint> frame_index, Expr<uint2> pixel_id, Expr<float> time) const noexcept
{
    sampler()->start(pixel_id, frame_index);

    auto u_filter        = sampler()->generate_2d();
    auto u_lens          = camera->base()->requires_lens_sampling() ? sampler()->generate_2d() : make_float2(0.5f);
    auto [camera_ray, _] = camera->generate_ray(pixel_id, time, u_filter, u_lens);

    auto ray = camera_ray;

    Float3 radiance   = make_float3(0.0f);
    Float3 throughput = make_float3(1.0f);

    const auto MAX_DEPTH = 10u;
    const auto T_MIN     = 1e-5f;
    const auto T_MAX     = 1e10f;

    $for(depth, MAX_DEPTH)
    {
        auto wo = -ray->direction();
        auto it = renderer().geometry()->intersect(ray);

        $if(!it->valid())
        {
            radiance += throughput * make_float3(0.0f);
            $break;
        };

        Float3 attenuation = make_float3(0.0f);
        auto u_lobe        = sampler()->generate_1d();
        auto u             = sampler()->generate_2d();

        PolymorphicCall<Surface::Closure> call;
        renderer().surfaces().dispatch(it->shape.surface_tag(), [&](auto material) noexcept
        {
            material->closure(call, *it, time);
        });
        Bool scattered;
        Float3 emission = make_float3(0.0f);
        call.execute([&](auto closure) noexcept
        {
            scattered = closure->scatter(ray, attenuation, u, u_lobe);
            emission  = closure->emitted();
            radiance += throughput * emission;
            throughput *= attenuation;
        });

        $if(!scattered)
        {
            $break;
        };
    };

    Float3 color = radiance;

    return color;
};

} // namespace Yutrel