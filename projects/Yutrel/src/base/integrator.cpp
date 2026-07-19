#include "integrator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

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

PathIntegrator::PathIntegrator(uint max_depth) noexcept
    : _max_depth{max_depth}
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
    return builder.emplace<Integrator, PathIntegrator>(_max_depth);
}

void PathIntegrator::Instance::render(Stream& stream, bool enable_display)
{
    CommandBuffer command_buffer{stream};

    auto camera      = renderer().camera();
    auto resolution  = camera->film()->base()->resolution();
    auto pixel_count = resolution.x * resolution.y;

    renderer().reset_diagnostics(command_buffer);
    camera->film()->prepare(command_buffer, enable_display);
    bool output_saved = false;
    RenderDiagnostics diagnostics{};
    uint64_t host_nan_count{};
    uint64_t host_inf_count{};
    auto output_path = camera->film()->base()->filename();
    {
        render_one_camera(command_buffer, camera);
        if (camera->film()->should_close())
        {
            camera->film()->release();
            return;
        }
        luisa::vector<float4> pixels(pixel_count);
        camera->film()->download(command_buffer, pixels.data());
        std::array<uint, 4u> diagnostic_values{};
        renderer().download_diagnostics(command_buffer, diagnostic_values);
        command_buffer << synchronize();

        diagnostics = RenderDiagnostics{
            .path_nan = diagnostic_values[0u],
            .path_inf = diagnostic_values[1u],
            .film_nan = diagnostic_values[2u],
            .film_inf = diagnostic_values[3u],
        };
        for (auto& pixel : pixels)
        {
            auto values = reinterpret_cast<float*>(&pixel);
            for (auto channel = 0u; channel < 3u; channel++)
            {
                if (std::isnan(values[channel]))
                {
                    host_nan_count++;
                    values[channel] = 0.0f;
                }
                else if (std::isinf(values[channel]))
                {
                    host_inf_count++;
                    values[channel] = 0.0f;
                }
            }
        }
        Clock clock_save;
        output_saved = save_image(output_path, reinterpret_cast<const float*>(pixels.data()), resolution);
        if (output_saved)
        {
            LUISA_INFO("Saved render output '{}' in {} ms.", output_path.string(), clock_save.toc());
        }
    }
    camera->film()->release();

    auto invalid_count = diagnostics.total() + host_nan_count + host_inf_count;
    if (invalid_count != 0u)
    {
        LUISA_WARNING(
            "Non-finite render values: path NaN={}, path Inf={}, film NaN={}, film Inf={}, output NaN={}, output Inf={}.",
            diagnostics.path_nan,
            diagnostics.path_inf,
            diagnostics.film_nan,
            diagnostics.film_inf,
            host_nan_count,
            host_inf_count);
    }
    if (!output_saved)
    {
        throw std::runtime_error{luisa::format("Failed to save render output '{}'.", output_path.string()).c_str()};
    }
    if (invalid_count != 0u)
    {
        throw std::runtime_error{luisa::format(
                                     "Render failed with {} non-finite value(s). Debug output was saved to '{}'.",
                                     invalid_count,
                                     output_path.string())
                                     .c_str()};
    }
}

void PathIntegrator::Instance::render_interactive(Stream& stream)
{
    CommandBuffer command_buffer{stream};

    auto camera     = renderer().camera();
    auto resolution = camera->film()->base()->resolution();

    LUISA_INFO(
        "Interactive rendering at {}x{}, sampler_spp={}, seed={}.",
        resolution.x,
        resolution.y,
        sampler()->base()->spp(),
        sampler()->base()->seed());

    renderer().reset_diagnostics(command_buffer);
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
    LUISA_INFO("Start compiling interactive Integrator shader.");
    Clock clock_compile;
    auto render = renderer().device().compile(render_kernel);
    LUISA_INFO("Interactive Integrator shader compiled in {} ms.", clock_compile.toc());

    uint global_sample_index = 0u;
    LUISA_INFO("Interactive rendering started.");
    Clock clock_render;

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
    LUISA_INFO(
        "Interactive rendering finished after {} samples in {} ms.",
        global_sample_index,
        clock_render.toc());
}

void PathIntegrator::Instance::render_one_camera(CommandBuffer& command_buffer, Camera::Instance* camera)
{
    auto spp        = sampler()->base()->spp();
    auto resolution = camera->film()->base()->resolution();

    sampler()->reset(command_buffer, resolution, resolution.x * resolution.y);
    command_buffer << synchronize();

    LUISA_INFO(
        "Rendering to '{}' at {}x{} and {} spp.",
        camera->film()->base()->filename().string(),
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

    LUISA_INFO("Start compiling Integrator shader.");
    Clock clock_compile;
    auto render = renderer().device().compile(render_kernel);
    LUISA_INFO("Integrator shader compiled in {} ms.", clock_compile.toc());
    command_buffer << synchronize();

    auto shutter_samples = camera->base()->shutter_samples(spp, sampler()->base()->seed());
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
    auto eta_scale = def(1.0f);

    auto ray          = camera_ray;
    auto pdf_bsdf     = def(1e16f);
    auto delta_bounce = def(true);
    auto depth = def(0u);
    $loop
    {
        // trace
        auto wo = -ray->direction();

        luisa::shared_ptr<Interaction> it = renderer().geometry()->intersect(ray);

        // miss
        $if(!it->valid())
        {
            if (renderer().environment() != nullptr)
            {
                auto eval   = light_sampler()->evaluate_miss(ray->direction(), swl, time);
                auto weight = ite(
                    (depth == 0u) | delta_bounce,
                    1.0f,
                    power_heuristic(pdf_bsdf, eval.pdf));
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
                    auto eval   = light_sampler()->evaluate_hit(*it, ray->origin(), swl, time);
                    auto weight = ite(delta_bounce, 1.0f, power_heuristic(pdf_bsdf, eval.pdf));
                    Li += beta * eval.L * weight;
                };
            };
        };

        // Match PBRT-v4 maxdepth semantics: evaluate emission at the terminal
        // vertex, then stop before direct lighting or another scattering event.
        $if(depth == max_depth()) { $break; };

        // no surface
        $if(!it->shape.has_surface()) { $break; };

        depth += 1u;

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

        $outline
        {
            PolymorphicCall<Surface::Closure> call;
            renderer().surfaces().dispatch(it->shape.surface_tag(), [&](auto surface) noexcept
            {
                surface->closure(call, *it, wo, swl, time, 1.0f);
            });
            call.execute([&](const Surface::Closure* closure) noexcept
            {
                // direct lighting
                $if(light_sample.eval.pdf > 0.0f & !occluded)
                {
                    auto wi   = light_sample.shadow_ray->direction();
                    auto eval = closure->evaluate(wo, wi);
                    auto mis  = ite(light_sample.delta, 1.0f,
                                     power_heuristic(light_sample.eval.pdf, eval.pdf));
                    auto w    = mis / light_sample.eval.pdf;
                    Li += w * beta * eval.f * light_sample.eval.L;
                };

                // sample surface
                auto surface_sample = closure->sample(wo, u_lobe, u_bsdf);
                ray                 = it->spawn_ray(surface_sample.wi);
                pdf_bsdf            = surface_sample.pdf_mis;
                delta_bounce        = surface_sample.delta;
                auto w              = ite(surface_sample.eval.pdf > 0.0f, 1.0f / surface_sample.eval.pdf, 0.0f);
                beta *= w * surface_sample.eval.f;
                auto transmission = (surface_sample.event & Surface::event_transmit) != 0u;
                eta_scale *= ite(transmission, sqr(surface_sample.eta), 1.0f);
            });
        };

        auto beta_has_nan = beta.any([](const auto& value) noexcept
        {
            return compute::isnan(value);
        });
        auto beta_has_inf = beta.any([](const auto& value) noexcept
        {
            return compute::isinf(value);
        });
        auto beta_invalid = beta_has_nan | beta_has_inf;
        renderer().record_path_non_finite(beta_has_nan, beta_has_inf);
        beta = beta.map([&](auto value) noexcept
        {
            return ite(beta_invalid, 0.0f, value);
        });
        $if(beta.all([](auto b) noexcept
        {
            return b <= 0.0f;
        }))
        {
            $break;
        };

        // Match PBRT-v4: start RR after the second scattering event and base
        // the survival probability on throughput adjusted for refraction.
        auto rr_beta_max = beta.max() * eta_scale;
        $if((depth > 1u) & (rr_beta_max < 1.0f))
        {
            auto q = max(0.0f, 1.0f - rr_beta_max);
            auto u_rr = sampler()->generate_1d();
            $if(u_rr < q)
            {
                $break;
            };
            beta /= 1.0f - q;
        };
    };

    Float3 color = spectrum->srgb(swl, Li);

    return color;
};

} // namespace Yutrel
