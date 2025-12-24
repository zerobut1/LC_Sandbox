#include "integrator.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/film.h"
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

    void Integrator::render_one_camera(CommandBuffer& command_buffer, Camera* camera)
    {
        // temp
        //-------------------------
        luisa::vector<SphereData> host_spheres;
        host_spheres.reserve(256u);

        // ground
        auto mat_id = m_materials.create<Lambertian>(make_float3(137.0f / 255.0f, 227.0f / 255.0f, 78.0f / 255.0f));
        host_spheres.emplace_back(SphereData{make_float3(0.0f, -1000.0f, 0.0f), 1000.0f, make_float3(0.0f), mat_id});

        // small spheres
        for (int a = -11; a < 11; a++)
        {
            for (int b = -11; b < 11; b++)
            {
                auto choose_mat = random_double();
                float3 center(static_cast<float>(a) + 0.9f * static_cast<float>(random_double()), 0.2f, static_cast<float>(b) + 0.9f * static_cast<float>(random_double()));

                if (length(center - float3(4.0f, 0.2f, 0.0f)) > 0.9f)
                {
                    if (choose_mat < 0.8) // diffuse
                    {
                        float3 albedo = make_float3(static_cast<float>(random_double()) * static_cast<float>(random_double()),
                                                    static_cast<float>(random_double()) * static_cast<float>(random_double()),
                                                    static_cast<float>(random_double()) * static_cast<float>(random_double()));
                        auto mat_id   = m_materials.create<Lambertian>(albedo);
                        auto velocity = make_float3(0.0f, 0.5f * static_cast<float>(random_double()), 0.0f);
                        host_spheres.emplace_back(SphereData{center, 0.2f, velocity, mat_id});
                    }
                    else if (choose_mat < 0.95) // metal
                    {
                        float3 albedo = make_float3(0.5f * (1.0f + static_cast<float>(random_double())),
                                                    0.5f * (1.0f + static_cast<float>(random_double())),
                                                    0.5f * (1.0f + static_cast<float>(random_double())));
                        float fuzz    = 0.5f * static_cast<float>(random_double());
                        auto mat_id   = m_materials.create<Metal>(albedo, fuzz);
                        host_spheres.emplace_back(SphereData{center, 0.2f, make_float3(0.0f), mat_id});
                    }
                    else // glass
                    {
                        auto mat_id = m_materials.create<Dielectric>(1.5f);
                        auto radius = static_cast<float>(random_double());
                        center.y    = radius;
                        host_spheres.emplace_back(SphereData{center, radius, make_float3(0.0f), mat_id});
                    }
                }
            }
        }

        auto sphere_buffer = renderer().device().create_buffer<SphereData>(host_spheres.size());
        command_buffer << sphere_buffer.copy_from(host_spheres.data()) << commit();
        auto sphere_buffer_view = sphere_buffer.view();
        auto sphere_count       = static_cast<uint>(host_spheres.size());
        m_world                 = luisa::make_unique<HittableList>(sphere_buffer_view, sphere_count);
        //-------------------------

        auto spp        = camera->spp();
        auto resolution = camera->film()->resolution();

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

        auto shutter_samples = camera->shutter_samples();
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

    Float3 Integrator::Li(const Camera* camera, Expr<uint> frame_index, Expr<uint2> pixel_id, Expr<float> time) const noexcept
    {
        sampler()->start(pixel_id, frame_index);

        auto u_filter        = sampler()->generate_2d();
        auto u_lens          = camera->requires_lens_sampling() ? sampler()->generate_2d() : make_float2(0.5f);
        auto [camera_ray, _] = camera->generate_ray(pixel_id, time, u_filter, u_lens);

        Float3 color = ray_color(camera_ray, time);

        return color;
    };

    Float3 Integrator::ray_color(Var<Ray> ray, Expr<float> time) const noexcept
    {
        Float3 color = make_float3(1.0f);

        const auto MAX_DEPTH = 10u;
        const auto T_MIN     = 0.0001f;
        const auto T_MAX     = 1e10f;

        $for(depth, MAX_DEPTH)
        {
            HitRecord rec;
            $if(!m_world->hit(ray, T_MIN, T_MAX, time, rec))
            {
                // background color
                auto direction = normalize(ray->direction());
                auto a         = 0.5f * (direction.y + 1.0f);
                color *= lerp(make_float3(1.0f, 1.0f, 1.0f), make_float3(0.4f, 0.8f, 1.0f), a);
                $break;
            };

            Float3 attenuation = make_float3(0.0f);
            auto u_lobe        = sampler()->generate_1d();
            auto u             = sampler()->generate_2d();

            m_materials.dispatch(rec.mat_id, [&](auto material) noexcept
            {
                material->scatter(ray, rec, attenuation, u, u_lobe);
            });

            color *= attenuation;
        };

        return color;
    }

} // namespace Yutrel