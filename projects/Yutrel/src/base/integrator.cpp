#include "integrator.h"

#include <algorithm>

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/camera_controller.h"
#include "base/film.h"
#include "base/geometry.h"
#include "base/interaction.h"
#include "base/light_sampler.h"
#include "base/renderer.h"
#include "base/sampler.h"
#include "scene/scene_builder.h"
#include "utils/command_buffer.h"
#include "utils/image_io.h"
#include "utils/progress_bar.h"
#include "utils/sampling.h"
#include "utils/spectra.h"

namespace Yutrel
{
Integrator::Instance::Instance(Renderer& renderer, CommandBuffer& command_buffer, const Integrator* integrator, const Sampler* sampler) noexcept
    : _renderer{renderer},
      _integrator{integrator},
      _sampler{sampler->build(renderer)},
      _light_sampler{LightSampler::create(renderer, command_buffer)}
{
}

Integrator::Instance::~Instance() noexcept = default;

PathIntegrator::PathIntegrator(uint max_depth, uint rr_depth, float rr_threshold) noexcept
    : _max_depth{max_depth},
      _rr_depth{rr_depth},
      _rr_threshold{std::max(rr_threshold, 0.05f)}
{
}

PathIntegrator::Instance::Instance(Renderer& renderer, CommandBuffer& command_buffer, const PathIntegrator* integrator, const Sampler* sampler) noexcept
    : Integrator::Instance{renderer, command_buffer, integrator, sampler}
{
}

luisa::unique_ptr<Integrator::Instance> PathIntegrator::build(Renderer& renderer, CommandBuffer& command_buffer, const Sampler* sampler) const noexcept
{
    return luisa::make_unique<PathIntegrator::Instance>(renderer, command_buffer, this, sampler);
}

const Integrator* PathIntegratorSpec::build(SceneBuilder& builder) const noexcept
{
    return builder.emplace<Integrator, PathIntegrator>(_max_depth, _rr_depth, _rr_threshold);
}

void PathIntegrator::Instance::render(Stream& stream, bool enable_display)
{
    CommandBuffer command_buffer{stream};

    auto camera      = renderer().camera();
    auto resolution  = camera->film()->base()->resolution();
    auto pixel_count = resolution.x * resolution.y;

    camera->film()->prepare(command_buffer, enable_display);
    {
        render_one_camera(command_buffer, camera);
        if (camera->film()->should_close())
        {
            camera->film()->release();
            return;
        }
        luisa::vector<float4> pixels(pixel_count);
        camera->film()->download(command_buffer, pixels.data());
        command_buffer << synchronize();
        auto output_path = camera->film()->base()->filename();
        save_image(output_path, reinterpret_cast<const float*>(pixels.data()), resolution);
    }
    camera->film()->release();
}

void PathIntegrator::Instance::render_interactive(Stream& stream)
{
    CommandBuffer command_buffer{stream};

    auto camera     = renderer().camera();
    auto resolution = camera->film()->base()->resolution();

    camera->film()->prepare(command_buffer, true);
    sampler()->reset(command_buffer, resolution, resolution.x * resolution.y);
    command_buffer << synchronize();

    FpsCameraController controller{camera->transform(), camera->base()->up(), FpsCameraController::Config{}};

    Kernel2D render_kernel = [&](UInt frame_index, Float time) noexcept
    {
        set_block_size(16u, 16u, 1u);
        Var pixel_id = dispatch_id().xy();
        Var L        = Li(camera, frame_index, pixel_id, time);
        camera->film()->accumulate_single_writer(pixel_id, L, 1.0f);
    };
    auto render = renderer().device().compile(render_kernel);

    uint global_sample_index = 0u;

    while (true)
    {
        // Process window events & draw current accumulation.
        camera->film()->show(command_buffer, true);
        if (camera->film()->should_close())
        {
            break;
        }

        // Update camera from input; reset accumulation if changed.
        if (controller.update())
        {
            auto c2w = controller.camera_to_world();
            camera->set_transform(command_buffer, c2w);
            camera->film()->prepare(command_buffer, true);
            sampler()->reset(command_buffer, resolution, resolution.x * resolution.y);
            global_sample_index = 0u;
            command_buffer << synchronize();
        }

        command_buffer
            << render(global_sample_index++, 0.0f).dispatch(resolution)
            << commit();
    }

    command_buffer << synchronize();
    camera->film()->release();
}

void PathIntegrator::Instance::render_one_camera(CommandBuffer& command_buffer, Camera::Instance* camera)
{
    auto spp        = sampler()->base()->spp();
    auto resolution = camera->film()->base()->resolution();

    sampler()->reset(command_buffer, resolution, resolution.x * resolution.y);
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
        camera->film()->accumulate_single_writer(pixel_id, L * shutter_weight, 1.0f);
    };

    LUISA_INFO("Start compiling Integrator shader");
    Clock clock_compile;
    auto render = renderer().device().compile(render_kernel);
    LUISA_INFO("Integrator shader compile in {} ms.", clock_compile.toc());
    command_buffer << synchronize();

    auto shutter_samples = camera->base()->shutter_samples(spp);
    LUISA_INFO("Rendering started.");
    Clock clock_render;
    ProgressBar progress_bar;
    progress_bar.update(0.0);
    constexpr auto dispatches_per_commit = 4u;
    constexpr auto max_progress_updates  = 100u;
    auto progress_stride                 = std::max(1u, (spp + max_progress_updates - 1u) / max_progress_updates);
    auto dispatch_count                  = 0u;
    auto global_sample_index             = 0u;
    for (const auto& s : shutter_samples)
    {
        for (auto i = 0u; i < s.spp; i++)
        {
            dispatch_count++;
            command_buffer << render(global_sample_index++, s.time, s.weight).dispatch(resolution);

            if (camera->film()->show(command_buffer))
            {
                dispatch_count = 0u;
            }
            if (camera->film()->should_close()) [[unlikely]]
            {
                command_buffer << synchronize();
                progress_bar.cancel();
                return;
            }

            auto progress_due = global_sample_index < spp && global_sample_index % progress_stride == 0u;
            if (progress_due) [[unlikely]]
            {
                dispatch_count = 0u;
                auto p         = global_sample_index / static_cast<double>(spp);
                command_buffer << [&progress_bar, p]
                {
                    progress_bar.update(p);
                };
            }
            else if (dispatch_count >= dispatches_per_commit) [[unlikely]]
            {
                dispatch_count = 0u;
                command_buffer << commit();
            }
        }
    }
    command_buffer << synchronize();
    progress_bar.done();
    LUISA_INFO("Rendering finished in {} ms.", clock_render.toc());
}

Float3 PathIntegrator::Instance::Li(const Camera::Instance* camera, Expr<uint> frame_index, Expr<uint2> pixel_id, Expr<float> time) const noexcept
{
    sampler()->start(pixel_id, frame_index);

    auto u_filter = sampler()->generate_pixel_2d();
    auto u_lens   = camera->base()->requires_lens_sampling() ? sampler()->generate_2d() : make_float2(0.5f);

    auto [camera_ray, _, camera_weight] = camera->generate_ray(pixel_id, time, u_filter, u_lens);

    auto spectrum = renderer().spectrum();
    auto swl      = spectrum->sample(spectrum->base()->is_fixed() ? 0.0f : sampler()->generate_1d());
    SampledSpectrum Li{swl.dimension(), 0.0f};
    SampledSpectrum beta{swl.dimension(), camera_weight};

    auto ray      = camera_ray;
    auto pdf_bsdf = def(1e16f);
    $for(depth, max_depth())
    {
        // trace
        auto wo = -ray->direction();

        luisa::shared_ptr<Interaction> it = renderer().geometry()->intersect(ray);

        // miss
        $if(!it->valid())
        {
            if (renderer().environment() != nullptr)
            {
                auto eval = light_sampler()->evaluate_miss(ray->direction(), swl, time);
                auto weight = ite(
                    depth == 0u,
                    1.0f,
                    balance_heuristic(pdf_bsdf, eval.pdf));
                Li += beta * eval.L * weight;
            }
            $break;
        };

        // hit light
        $if(!renderer().lights().empty())
        {
            $outline
            {
                $if(it->shape.has_light())
                {
                    auto eval = light_sampler()->evaluate_hit(*it, ray->origin(), swl, time);
                    Li += beta * eval.L * balance_heuristic(pdf_bsdf, eval.pdf);
                };
            };
        };

        // no surface
        $if(!it->shape.has_surface()) { $break; };

        // sample light
        auto u_light_selection = sampler()->generate_1d();
        auto u_light_surface   = sampler()->generate_2d();
        auto light_sample      = LightSampler::Sample::zero(swl.dimension());
        $outline
        {
            light_sample = light_sampler()->sample(*it, u_light_selection, u_light_surface, swl, time);
        };

        // cast shadow ray
        auto occluded = def(false);
        if (renderer().has_lighting())
        {
            occluded = renderer().geometry()->intersect_any(light_sample.shadow_ray);
        }

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
                surface->closure(call, *it, wo, swl, time);
            });
            call.execute([&](const Surface::Closure* closure) noexcept
            {
                // direct lighting
                $if(light_sample.eval.pdf > 0.0f & !occluded)
                {
                    auto wi   = light_sample.shadow_ray->direction();
                    auto eval = closure->evaluate(wo, wi);
                    auto w    = balance_heuristic(light_sample.eval.pdf, eval.pdf) / light_sample.eval.pdf;
                    Li += w * beta * eval.f * light_sample.eval.L;
                };

                // sample surface
                auto surface_sample = closure->sample(wo, u_lobe, u_bsdf);
                ray                 = it->spawn_ray(surface_sample.wi);
                pdf_bsdf            = surface_sample.eval.pdf;
                auto w              = ite(surface_sample.eval.pdf > 0.0f, 1.0f / surface_sample.eval.pdf, 0.0f);
                beta *= w * surface_sample.eval.f;
            });
        };

        beta = zero_if_any_nan(beta);
        $if(beta.all([](auto b) noexcept
        {
            return b <= 0.0f;
        }))
        {
            $break;
        };

        auto q = max(beta.max(), 0.05f);
        $if(depth + 1u >= rr_depth())
        {
            $if(q < rr_threshold() & u_rr >= q)
            {
                $break;
            };
            beta *= ite(q < rr_threshold(), 1.0f / q, 1.0f);
        };
    };

    Float3 color = spectrum->srgb(swl, Li);

    return color;
};

} // namespace Yutrel
