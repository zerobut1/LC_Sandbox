#include "renderer.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/integrator.h"
#include "base/scene.h"

namespace Yutrel
{
    Renderer::Renderer(Device& device) noexcept
        : m_device(device) {}

    Renderer::~Renderer() noexcept = default;

    luisa::unique_ptr<Renderer> Renderer::create(Device& device, Stream& stream, const Scene& scene) noexcept
    {
        auto renderer = luisa::make_unique<Renderer>(device);

        CommandBuffer command_buffer{stream};

        renderer->m_camera     = scene.camera()->build(*renderer, command_buffer);
        renderer->m_integrator = Integrator::create(*renderer);

        // temp
        //-------------------------
        luisa::vector<SphereData> host_spheres;
        host_spheres.reserve(256u);

        // ground
        auto mat_id = renderer->m_materials.emplace(Lambertian(make_float3(137.0f / 255.0f, 227.0f / 255.0f, 78.0f / 255.0f)).build(*renderer, command_buffer));
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
                        auto mat_id   = renderer->m_materials.emplace(Lambertian(albedo).build(*renderer, command_buffer));
                        auto velocity = make_float3(0.0f, 0.5f * static_cast<float>(random_double()), 0.0f);
                        host_spheres.emplace_back(SphereData{center, 0.2f, velocity, mat_id});
                    }
                    else if (choose_mat < 0.95) // metal
                    {
                        float3 albedo = make_float3(0.5f * (1.0f + static_cast<float>(random_double())),
                                                    0.5f * (1.0f + static_cast<float>(random_double())),
                                                    0.5f * (1.0f + static_cast<float>(random_double())));
                        float fuzz    = 0.5f * static_cast<float>(random_double());
                        auto mat_id   = renderer->m_materials.emplace(Metal(albedo, fuzz).build(*renderer, command_buffer));
                        host_spheres.emplace_back(SphereData{center, 0.2f, make_float3(0.0f), mat_id});
                    }
                    else // glass
                    {
                        auto mat_id = renderer->m_materials.emplace(Dielectric(1.5f).build(*renderer, command_buffer));
                        auto radius = static_cast<float>(random_double());
                        center.y    = radius;
                        host_spheres.emplace_back(SphereData{center, radius, make_float3(0.0f), mat_id});
                    }
                }
            }
        }

        auto sphere_buffer = renderer->create<Buffer<SphereData>>(host_spheres.size());
        command_buffer << sphere_buffer->copy_from(host_spheres.data()) << commit();
        auto sphere_buffer_view = sphere_buffer->view();
        auto sphere_count       = static_cast<uint>(host_spheres.size());
        renderer->m_world       = luisa::make_unique<HittableList>(sphere_buffer_view, sphere_count);
        //-------------------------

        command_buffer << synchronize();

        return renderer;
    }

    void Renderer::render(Stream& stream)
    {
        m_integrator->render(stream);
    }

} // namespace Yutrel