#include "renderer.h"

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/integrator.h"
#include "base/scene.h"

namespace Yutrel
{
    Renderer::Renderer(Device& device) noexcept
        : m_device(device),
          m_bindless_array(device.create_bindless_array()) {}

    Renderer::~Renderer() noexcept = default;

    void Renderer::scene_spheres(Scene& scene, CommandBuffer& command_buffer) noexcept
    {
        luisa::vector<SphereData> host_spheres;
        host_spheres.reserve(256u);

        // ground
        Texture::CreateInfo ground_info{
            .type  = Texture::Type::checker_board,
            .scale = 0.32f,
            .even  = make_float4(0.2f, 0.3f, 0.1f, 1.0f),
            .odd   = make_float4(0.9f, 0.9f, 0.9f, 1.0f),
        };
        auto mat_id = m_materials.emplace(Lambertian(scene, ground_info).build(*this, command_buffer));
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
                        Texture::CreateInfo texture_info{.v = make_float4(albedo, 1.0f)};
                        auto mat_id   = m_materials.emplace(Lambertian(scene, texture_info).build(*this, command_buffer));
                        auto velocity = make_float3(0.0f, 0.5f * static_cast<float>(random_double()), 0.0f);
                        host_spheres.emplace_back(SphereData{center, 0.2f, velocity, mat_id});
                    }
                    else if (choose_mat < 0.95) // metal
                    {
                        float3 albedo = make_float3(0.5f * (1.0f + static_cast<float>(random_double())),
                                                    0.5f * (1.0f + static_cast<float>(random_double())),
                                                    0.5f * (1.0f + static_cast<float>(random_double())));
                        float fuzz    = 0.5f * static_cast<float>(random_double());
                        auto mat_id   = m_materials.emplace(Metal(scene, albedo, fuzz).build(*this, command_buffer));
                        host_spheres.emplace_back(SphereData{center, 0.2f, make_float3(0.0f), mat_id});
                    }
                    else // glass
                    {
                        auto mat_id = m_materials.emplace(Dielectric(1.5f).build(*this, command_buffer));
                        auto radius = static_cast<float>(random_double());
                        center.y    = radius;
                        host_spheres.emplace_back(SphereData{center, radius, make_float3(0.0f), mat_id});
                    }
                }
            }
        }

        auto sphere_buffer = create<Buffer<SphereData>>(host_spheres.size());
        command_buffer << sphere_buffer->copy_from(host_spheres.data()) << commit();
        auto sphere_buffer_view = sphere_buffer->view();
        auto sphere_count       = static_cast<uint>(host_spheres.size());
        m_world                 = luisa::make_unique<HittableList>(sphere_buffer_view, sphere_count);

        // camera
        Camera::CreateInfo camera_info{
            .type                  = Camera::Type::thin_lens,
            .spp                   = 1024u,
            .shutter_span          = {0.0f, 1.0f},
            .shutter_samples_count = 64u,
            .position              = make_float3(13.0f, 2.0f, 3.0f),
            .lookat                = make_float3(0.0f, 0.0f, 0.0f),
            .up                    = make_float3(0.0f, 1.0f, 0.0f),
            // pinhole
            .fov = 20.0f,
            // thin lens
            .aperture       = 0.4f,
            .focal_length   = 78.0f,
            .focus_distance = 13.0f,
        };
        scene.load_camera(camera_info);
    }

    void Renderer::scene_sphere(Scene& scene, CommandBuffer& command_buffer) noexcept
    {
        luisa::vector<SphereData> host_spheres;
        host_spheres.reserve(256u);

        Texture::CreateInfo texture_info{
            .type     = Texture::Type::image,
            .path     = "scene/common/textures/image.png",
            .sampler  = TextureSampler::anisotropic_repeat(),
            .encoding = Texture::Encoding::SRGB,
        };
        auto mat_id = m_materials.emplace(Lambertian(scene, texture_info).build(*this, command_buffer));
        host_spheres.emplace_back(SphereData{make_float3(0.0f, 0.0f, 0.0f), 2.0f, make_float3(0.0f), mat_id});

        auto sphere_buffer = create<Buffer<SphereData>>(host_spheres.size());
        command_buffer << sphere_buffer->copy_from(host_spheres.data()) << commit();
        auto sphere_buffer_view = sphere_buffer->view();
        auto sphere_count       = static_cast<uint>(host_spheres.size());
        m_world                 = luisa::make_unique<HittableList>(sphere_buffer_view, sphere_count);

        // camera
        Camera::CreateInfo camera_info{
            .type                  = Camera::Type::pinhole,
            .spp                   = 1024u,
            .shutter_span          = {0.0f, 1.0f},
            .shutter_samples_count = 64u,
            .position              = make_float3(0.0f, 0.0f, 12.0f),
            .lookat                = make_float3(0.0f, 0.0f, 0.0f),
            .up                    = make_float3(0.0f, 1.0f, 0.0f),
            // pinhole
            .fov = 20.0f,
        };
        scene.load_camera(camera_info);
    }

    luisa::unique_ptr<Renderer> Renderer::create(Device& device, Stream& stream, Scene& scene) noexcept
    {
        auto renderer = luisa::make_unique<Renderer>(device);

        CommandBuffer command_buffer{stream};

        //-------------------------
        switch (2)
        {
        case 1:
            renderer->scene_spheres(scene, command_buffer);
            break;
        case 2:
        default:
            renderer->scene_sphere(scene, command_buffer);
            break;
        }
        //-------------------------

        if (renderer->m_bindless_array.dirty())
        {
            command_buffer << renderer->m_bindless_array.update();
        }

        renderer->m_camera     = scene.camera()->build(*renderer, command_buffer);
        renderer->m_integrator = Integrator::create(*renderer);

        command_buffer << synchronize();

        return renderer;
    }

    void Renderer::render(Stream& stream)
    {
        m_integrator->render(stream);
    }

    const Texture::Instance* Renderer::build_texture(CommandBuffer& command_buffer, const Texture* texture) noexcept
    {
        if (texture == nullptr)
            return nullptr;
        if (auto iter = m_textures.find(texture); iter != m_textures.end())
        {
            return iter->second.get();
        }
        auto t = texture->build(*this, command_buffer);
        return m_textures.emplace(texture, std::move(t)).first->second.get();
    }

} // namespace Yutrel