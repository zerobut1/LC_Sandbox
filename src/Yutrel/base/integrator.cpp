#include "integrator.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/film.h"
#include "base/renderer.h"
#include "base/sampler.h"
#include "utils/command_buffer.h"

#include <random>

namespace Yutrel
{
    Integrator::Integrator(Renderer& renderer) noexcept
        : m_renderer{&renderer},
          m_sampler(Sampler::create(m_renderer))
    {
    }

    Integrator::~Integrator() noexcept = default;

    luisa::unique_ptr<Integrator> Integrator::create(Renderer& renderer) noexcept
    {
        return luisa::make_unique<Integrator>(renderer);
    }

    void Integrator::render(Stream& stream)
    {
        CommandBuffer command_buffer{&stream};

        auto camera = m_renderer->camera();

        camera->film()->prepare(command_buffer);
        {
            render_one_camera(command_buffer, camera);
        }
        camera->film()->release();
    }

    double random_double()
    {
        static std::uniform_real_distribution<double> distribution(0.0, 1.0);
        static std::mt19937 generator(std::random_device{}());
        return distribution(generator);
    }

    void Integrator::render_one_camera(CommandBuffer& command_buffer, Camera* camera)
    {
        // temp
        //-------------------------
        HittableList world;

        // ground
        materials.emplace_back(luisa::make_unique<Lambertian>(make_float3(137.0f / 255.0f, 227.0f / 255.0f, 78.0f / 255.0f)));
        world.add(luisa::make_shared<Sphere>(make_float3(0.0f, -1000.0f, 0.0f), 1000.0f, materials.size() - 1));

        // small spheres
        for (int a = -5; a < 5; a++)
        {
            for (int b = -5; b < 5; b++)
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
                        materials.emplace_back(make_unique<Lambertian>(albedo));
                        auto velocity = make_float3(0.0f, 0.5f * static_cast<float>(random_double()), 0.0f);
                        world.add(luisa::make_shared<Sphere>(center, 0.2f, velocity, materials.size() - 1));
                    }
                    else if (choose_mat < 0.95) // metal
                    {
                        float3 albedo = make_float3(0.5f * (1.0f + static_cast<float>(random_double())),
                                                    0.5f * (1.0f + static_cast<float>(random_double())),
                                                    0.5f * (1.0f + static_cast<float>(random_double())));
                        float fuzz    = 0.5f * static_cast<float>(random_double());
                        materials.emplace_back(make_unique<Metal>(albedo, fuzz));
                        world.add(luisa::make_shared<Sphere>(center, 0.2f, materials.size() - 1));
                    }
                    else // glass
                    {
                        materials.emplace_back(make_unique<Dielectric>(1.5f));
                        auto radius = static_cast<float>(random_double());
                        center.y     = radius;
                        world.add(luisa::make_shared<Sphere>(center, radius, materials.size() - 1));
                    }
                }
            }
        }
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
            Var L        = Li(camera, frame_index, pixel_id, time, world);
            camera->film()->accumulate(pixel_id, L * shutter_weight, 1.0f);
        };

        LUISA_INFO("Start compiling Integrator shader");
        Clock clock_compile;
        auto render = renderer()->device().compile(render_kernel);
        LUISA_INFO("Integrator shader compile in {} ms.", clock_compile.toc());
        command_buffer << synchronize();

        auto shutter_samples = camera->shutter_samples();
        LUISA_INFO("Rendering started.");
        Clock clock_render;
        auto dispatch_count      = 0u;
        auto global_sample_index = 0u;
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
                const auto dispatches_per_commit = 4u;
                if (dispatch_count >= dispatches_per_commit)
                {
                    dispatch_count = 0u;
                    command_buffer << commit();
                }
            }
        }
        command_buffer << synchronize();
        LUISA_INFO("Rendering finished in {} ms.", clock_render.toc());
    }

    Float3 Integrator::Li(const Camera* camera, Expr<uint> frame_index, Expr<uint2> pixel_id, Expr<float> time, HittableList& world) const noexcept
    {
        sampler()->start(pixel_id, frame_index);

        auto u_filter        = sampler()->generate_2d();
        auto u_lens          = camera->requires_lens_sampling() ? sampler()->generate_2d() : make_float2(0.5f);
        auto [camera_ray, _] = camera->generate_ray(pixel_id, time, u_filter, u_lens);

        Float3 color = ray_color(camera_ray, world, time);

        return color;
    };

    Float3 Integrator::ray_color(Var<Ray> ray, const Hittable& world, Expr<float> time) const noexcept
    {
        Float3 color = make_float3(1.0f);

        const auto MAX_DEPTH = 10u;
        const auto T_MIN     = 0.0001f;
        const auto T_MAX     = 1e10f;

        $for(depth, MAX_DEPTH)
        {
            HitRecord rec;
            $if(!world.hit(ray, T_MIN, T_MAX, time, rec))
            {
                // background color
                auto direction = normalize(ray->direction());
                auto a         = 0.5f * (direction.y + 1.0f);
                color *= lerp(make_float3(1.0f, 1.0f, 1.0f), make_float3(0.4f, 0.8f, 1.0f), a);
                $break;
            };

            Bool is_scattered = false;
            Float3 attenuation;
            for (uint mat_id = 0; mat_id < materials.size(); mat_id++)
            {
                $if(rec.mat_id == mat_id)
                {
                    auto u_lobe  = sampler()->generate_1d();
                    auto u       = sampler()->generate_2d();
                    is_scattered = materials[mat_id]->scatter(ray, rec, attenuation, u, u_lobe);
                };
            };

            $if(!is_scattered)
            {
                color = make_float3(0.0f);
                $break;
            };

            color *= attenuation;
        };

        return color;
    }

} // namespace Yutrel