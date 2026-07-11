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

class Renderer;

class Camera
{
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
        Instance(Renderer& renderer, CommandBuffer& command_buffer, const Camera* camera, const Film* film, const Filter* filter) noexcept;
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
    float4x4 m_init_transform;
    float3 m_up;
    float2 m_shutter_span;
    uint m_shutter_samples_count;

public:
    Camera(float3 position, float3 lookat, float3 up, float2 shutter_span, uint shutter_samples_count) noexcept;
    virtual ~Camera() noexcept;

    Camera()                         = delete;
    Camera(const Camera&)            = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&)                 = delete;
    Camera& operator=(Camera&&)      = delete;

public:
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer, const Film* film, const Filter* filter) const noexcept = 0;

    [[nodiscard]] auto init_transform() const noexcept { return m_init_transform; }
    [[nodiscard]] auto up() const noexcept { return m_up; }
    [[nodiscard]] luisa::vector<ShutterSample> shutter_samples(uint spp) const noexcept;
    [[nodiscard]] virtual bool requires_lens_sampling() const noexcept { return false; }
};

} // namespace Yutrel
