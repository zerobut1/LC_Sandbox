#include "integrator.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/film.h"
#include "base/renderer.h"
#include "rtweekend/rtweekend.h"
#include "utils/command_buffer.h"

namespace Yutrel
{
    Integrator::Integrator(Renderer& renderer) noexcept
        : m_renderer{&renderer}
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
            render_one_frame(command_buffer, camera);
        }
        camera->film()->release();
    }

    void Integrator::render_one_frame(CommandBuffer& command_buffer, Camera* camera)
    {
        Kernel2D render_kernel = [&](UInt frame_index, Float time) noexcept
        {
            set_block_size(16u, 16u, 1u);
            Var pixel_id = dispatch_id().xy();
            Var L        = Li(camera, frame_index, pixel_id, time);
            camera->film()->accumulate(pixel_id, L);
        };

        Clock clock_compile;
        auto render = renderer()->device().compile(render_kernel);
        LUISA_INFO("Integrator shader compile in {} ms.", clock_compile.toc());
        command_buffer << synchronize();

        auto resolution = camera->film()->resolution();

        command_buffer << render(0u, 0.0f).dispatch(resolution);

        camera->film()->show(command_buffer);
    }

    Float3 Integrator::Li(const Camera* camera, UInt frame_index, UInt2 pixel_id, Float time) const noexcept
    {
        auto sample = camera->generate_ray(pixel_id);

        Float3 color = make_float3(0.0f);

        auto t = hit_sphere(make_float3(0.0f, 0.0f, -2.0f), 0.5f, sample.ray);

        $if(t >= 0.0f)
        {
            auto ray_direction = normalize(sample.ray->direction());
            auto hit_point     = sample.ray->origin() + ray_direction * t;
            auto N             = normalize(hit_point - make_float3(0.0f, 0.0f, -2.0f));
            device_log("ray direction: {}, {}, {}", ray_direction.x, ray_direction.y, ray_direction.z);
            color = N * 0.5f + 0.5f;
        }
        $else
        {
            auto direction = normalize(sample.ray->direction());
            auto a         = 0.5f * (direction.y + 1.0f);
            color          = lerp(make_float3(1.0f, 1.0f, 1.0f), make_float3(0.4f, 0.8f, 1.0f), a);
        };

        return color;
    };

} // namespace Yutrel