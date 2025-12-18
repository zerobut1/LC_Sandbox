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
        world.add(luisa::make_shared<Sphere>(make_float3(0.0f, 0.0f, -2.0f), 0.5f));
        world.add(luisa::make_shared<Sphere>(make_float3(0.0f, -100.5f, -2.0f), 100.0f));

        //-------------------------

        auto spp        = camera->spp();
        auto resolution = camera->film()->resolution();

        sampler()->reset(command_buffer, resolution.x * resolution.y);
        command_buffer << synchronize();

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

        for (auto i = 0u; i < spp; i++)
        {
            command_buffer << render(i, 0.0f).dispatch(resolution);
            camera->film()->show(command_buffer);
        }

        command_buffer << synchronize();
    }

    Float3 Integrator::Li(const Camera* camera, UInt frame_index, UInt2 pixel_id, Float time, HittableList& world) const noexcept
    {
        sampler()->start(pixel_id, frame_index);

        auto u_filter = sampler()->generate_2d();

        auto [camera_ray, _] = camera->generate_ray(pixel_id, u_filter);
        Float3 color         = make_float3(0.0f);

        HitRecord rec;
        $if(world.hit(camera_ray, 0, 1e10f, rec))
        {
            color = rec.normal * 0.5f + 0.5f;
        }
        $else
        {
            auto direction = normalize(camera_ray->direction());
            auto a         = 0.5f * (direction.y + 1.0f);
            color          = lerp(make_float3(1.0f, 1.0f, 1.0f), make_float3(0.4f, 0.8f, 1.0f), a);
        };

        return color;
    };

} // namespace Yutrel