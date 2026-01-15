#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>

#include "base/film.h"
#include "base/filter.h"
#include "utils/command_buffer.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Scene;
class Renderer;

class Camera
{
public:
    enum class Type
    {
        pinhole,
        thin_lens,
    };

    struct CreateInfo
    {
        Type type{Type::pinhole};

        // film
        Film::CreateInfo film_info{};

        // filter
        Filter::CreateInfo filter_info{};

        uint spp{1024u};
        float2 shutter_span{0.0f, 0.0f};
        uint shutter_samples_count{0u};

        float3 position{};
        float3 lookat{};
        float3 up{0.0f, 1.0f, 0.0f};

        // pinhole
        float fov{45.0f};

        // thin lens
        float aperture{2.0f};
        float focal_length{35.0f};
        float focus_distance{10.0f};
    };

    [[nodiscard]] static luisa::unique_ptr<Camera> create(Scene& scene, const CreateInfo& info) noexcept;

public:
    struct Sample
    {
        Var<Ray> ray;
        Float2 pixel;
        Float weight;
    };

    struct ShutterSample
    {
        float time;
        float weight;
        uint spp;
    };

public:
    class Instance
    {
    private:
        const Renderer& m_renderer;
        const Camera* m_camera;

        luisa::unique_ptr<Film::Instance> m_film;
        luisa::unique_ptr<Filter::Instance> m_filter;
        float4x4 m_host_transform;
        BufferView<float4x4> m_device_transform;

    public:
        explicit Instance(Renderer& renderer, CommandBuffer& command_buffer, const Camera* camera) noexcept;
        virtual ~Instance() noexcept = default;

        Instance()                           = delete;
        Instance(const Instance&)            = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&)                 = delete;
        Instance& operator=(Instance&&)      = delete;

    public:
        template <typename T = Camera>
            requires std::is_base_of_v<Camera, T>
        [[nodiscard]] auto base() const noexcept
        {
            return static_cast<const T*>(m_camera);
        }
        [[nodiscard]] auto film() const noexcept { return m_film.get(); }
        [[nodiscard]] auto filter() const noexcept { return m_filter.get(); }
        [[nodiscard]] auto transform() const noexcept { return m_host_transform; }

        void set_transform(CommandBuffer& command_buffer, const float4x4& c2w) noexcept;
        [[nodiscard]] Sample generate_ray(Expr<uint2> pixel_coord, Expr<float> time, Expr<float2> u_filter, Expr<float2> u_lens) const noexcept;

    private:
        [[nodiscard]] virtual Var<Ray> generate_ray_in_camera_space(Expr<float2> pixel, Expr<float> time, Expr<float2> u_lens) const noexcept = 0;
    };

private:
    const Film* m_film;
    const Filter* m_filter;
    float4x4 m_init_transform;
    float3 m_up;
    uint m_spp;
    float2 m_shutter_span;
    uint m_shutter_samples_count;

public:
    explicit Camera(Scene& scene, const CreateInfo& info) noexcept;
    virtual ~Camera() noexcept;

    Camera()                         = delete;
    Camera(const Camera&)            = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&)                 = delete;
    Camera& operator=(Camera&&)      = delete;

public:
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept = 0;

    [[nodiscard]] auto film() const noexcept { return m_film; }
    [[nodiscard]] auto filter() const noexcept { return m_filter; }
    [[nodiscard]] auto spp() const noexcept { return m_spp; }
    [[nodiscard]] auto init_transform() const noexcept { return m_init_transform; }
    [[nodiscard]] auto up() const noexcept { return m_up; }
    [[nodiscard]] luisa::vector<ShutterSample> shutter_samples() const noexcept;
    [[nodiscard]] virtual bool requires_lens_sampling() const noexcept { return false; }
};

} // namespace Yutrel