#include "integrator.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/film.h"
#include "base/geometry.h"
#include "base/interaction.h"
#include "base/light_sampler.h"
#include "base/renderer.h"
#include "base/sampler.h"
#include "utils/command_buffer.h"
#include "utils/image_io.h"
#include "utils/progress_bar.h"
#include "utils/sampling.h"

namespace Yutrel
{
luisa::unique_ptr<Integrator> Integrator::create(Renderer& renderer, CommandBuffer& command_buffer) noexcept
{
    return luisa::make_unique<Integrator>(renderer, command_buffer);
}

Integrator::Integrator(Renderer& renderer, CommandBuffer& command_buffer) noexcept
    : m_renderer(renderer),
      m_sampler(Sampler::create(renderer)),
      m_light_sampler(LightSampler::create(renderer, command_buffer)) {}

Integrator::~Integrator() noexcept = default;

void Integrator::render(Stream& stream)
{
    CommandBuffer command_buffer{stream};

    auto camera      = m_renderer.camera();
    auto resolution  = camera->film()->base()->resolution();
    auto pixel_count = resolution.x * resolution.y;

    camera->film()->prepare(command_buffer);
    {
        render_one_camera(command_buffer, camera);
        luisa::vector<float4> pixels(pixel_count);
        camera->film()->download(command_buffer, pixels.data());
        command_buffer << synchronize();
        auto output_path = std::filesystem::canonical(std::filesystem::current_path()) / "render.exr";
        save_image(output_path, reinterpret_cast<const float*>(pixels.data()), resolution);
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

    Float3 Li   = make_float3(0.0f);
    Float3 beta = make_float3(1.0f);

    auto ray      = camera_ray;
    auto pdf_bsdf = def(1e16f);
    $for(depth, max_depth())
    {
        // trace
        auto wo = -ray->direction();
        auto it = renderer().geometry()->intersect(ray);

        // miss
        $if(!it->valid())
        {
            // no environment light for now
            Li += beta * make_float3(0.0f);
            $break;
        };

        // hit light
        $if(!renderer().lights().empty())
        {
            $outline
            {
                $if(it->shape.has_light())
                {
                    auto eval = light_sampler()->evaluate_hit(*it, ray->origin(), time);
                    Li += beta * eval.L;
                };
            };
        };

        // no surface
        $if(!it->shape.has_surface()) { $break; };

        auto u_lobe = sampler()->generate_1d();
        auto u_bsdf = sampler()->generate_2d();

        auto u_rr = def(0.0f);
        $if(depth + 1u >= rr_depth())
        {
            u_rr = sampler()->generate_1d();
        };

        $outline
        {
            PolymorphicCall<Surface::Closure> call;
            renderer().surfaces().dispatch(it->shape.surface_tag(), [&](auto surface) noexcept
            {
                surface->closure(call, *it, time);
            });
            call.execute([&](auto closure) noexcept
            {
                auto surface_sample = closure->sample(wo, u_lobe, u_bsdf);
                ray                 = it->spawn_ray(surface_sample.wi);
                pdf_bsdf            = surface_sample.eval.pdf;
                auto w              = ite(surface_sample.eval.pdf > 0.0f, 1.0f / surface_sample.eval.pdf, 0.0f);
                beta *= w * surface_sample.eval.f;
            });
        };

        auto q = max(max(beta.x, beta.y), beta.z);
        q      = max(q, 0.05f);
        $if(depth + 1u >= rr_depth())
        {
            $if(u_rr >= q & q <= rr_threshold())
            {
                $break;
            };
            beta *= ite(q < rr_threshold(), 1.0f / q, 1.0f);
        };
    };

    Float3 color = Li;

    return color;
};

} // namespace Yutrel