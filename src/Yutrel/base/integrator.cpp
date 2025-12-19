#include "integrator.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/film.h"
#include "base/renderer.h"
#include "base/sampler.h"
#include "utils/command_buffer.h"

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

    void Integrator::render_one_camera(CommandBuffer& command_buffer, Camera* camera)
    {
        // temp
        //-------------------------
        HittableList world;

        // ground
        materials.emplace_back(luisa::make_unique<Lambertian>(make_float3(0.8, 0.8, 0.0f)));
        world.add(luisa::make_shared<Sphere>(make_float3(0.0f, -100.5f, -1.0f), 100.0f, materials.size() - 1));

        // center
        materials.emplace_back(luisa::make_unique<Lambertian>(make_float3(0.1f, 0.2f, 0.5f)));
        world.add(luisa::make_shared<Sphere>(make_float3(0.0f, 0.0f, -1.2f), 0.5f, materials.size() - 1));

        // left
        materials.emplace_back(luisa::make_unique<Dielectric>(1.5f));
        world.add(luisa::make_shared<Sphere>(make_float3(-1.0f, 0.0f, -1.0f), 0.5f, materials.size() - 1));

        // bubble
        materials.emplace_back(luisa::make_unique<Dielectric>(1.0f / 1.5f));
        world.add(luisa::make_shared<Sphere>(make_float3(-1.0f, 0.0f, -1.0f), 0.4f, materials.size() - 1));

        // right
        materials.emplace_back(luisa::make_unique<Metal>(make_float3(0.8f, 0.6f, 0.2f), 1.0f));
        world.add(luisa::make_shared<Sphere>(make_float3(1.0f, 0.0f, -1.0f), 0.5f, materials.size() - 1));

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

        Kernel2D render_kernel = [&](UInt frame_index, Float time) noexcept
        {
            set_block_size(16u, 16u, 1u);
            Var pixel_id = dispatch_id().xy();
            Var L        = Li(camera, frame_index, pixel_id, time, world);
            camera->film()->accumulate(pixel_id, L, 1.0f);
        };

        Clock clock_compile;
        auto render = renderer()->device().compile(render_kernel);
        LUISA_INFO("Integrator shader compile in {} ms.", clock_compile.toc());
        command_buffer << synchronize();

        LUISA_INFO("Rendering started.");
        Clock clock_render;
        auto dispatch_count = 0u;
        for (auto i = 0u; i < spp; i++)
        {
            dispatch_count++;
            command_buffer << render(i, 0.0f).dispatch(resolution);
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
        command_buffer << synchronize();
        LUISA_INFO("Rendering finished in {} ms.", clock_render.toc());
    }

    Float3 Integrator::Li(const Camera* camera, Expr<uint> frame_index, Expr<uint2> pixel_id, Expr<float> time, HittableList& world) const noexcept
    {
        sampler()->start(pixel_id, frame_index);

        auto u_filter        = sampler()->generate_2d();
        auto [camera_ray, _] = camera->generate_ray(pixel_id, u_filter);

        Float3 color = ray_color(camera_ray, world);

        return color;
    };

    Float3 Integrator::ray_color(Var<Ray> ray, const Hittable& world) const noexcept
    {
        Float3 color = make_float3(1.0f);

        const auto MAX_DEPTH = 50u;
        const auto T_MIN     = 0.0001f;
        const auto T_MAX     = 1e10f;

        $for(depth, MAX_DEPTH)
        {
            HitRecord rec;
            $if(!world.hit(ray, T_MIN, T_MAX, rec))
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