#pragma once

#include <luisa/dsl/syntax.h>
#include <luisa/runtime/rtx/accel.h>

#include "base/interaction.h"
#include "base/light.h"
#include "base/shape.h"
#include "utils/command_buffer.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Renderer;
class Shape;

class Geometry
{
public:
    struct MeshData
    {
        Mesh* resource;
        uint geometry_buffer_id_base : 22;
        uint vertex_properties : 10;
    };

    struct MeshGeometry
    {
        Mesh* resource;
        uint buffer_id_base;
    };

private:
    Renderer& m_renderer;
    Accel m_accel;
    uint m_triangle_count{0u};
    uint m_instanced_triangle_count{0u};
    luisa::unordered_map<const Shape*, MeshData> m_meshes;
    luisa::unordered_map<uint64_t, MeshGeometry> m_mesh_cache;
    luisa::vector<uint4> m_instances;
    Buffer<uint4> m_instance_buffer;
    luisa::vector<Light::Handle> m_instanced_lights;

public:
    explicit Geometry(Renderer& renderer) noexcept
        : m_renderer{renderer} {}

    void build(CommandBuffer& command_buffer, luisa::span<const Shape* const> shapes) noexcept;

    [[nodiscard]] auto instances() const noexcept { return luisa::span{m_instances}; }
    [[nodiscard]] auto light_instances() const noexcept { return luisa::span{m_instanced_lights}; }
    [[nodiscard]] Shape::Handle instance(Expr<uint> index) const noexcept;
    [[nodiscard]] Float4x4 instance_to_world(Expr<uint> index) const noexcept;
    [[nodiscard]] Var<Triangle> triangle(const Shape::Handle& instance, Expr<uint> index) const noexcept;
    [[nodiscard]] Var<TriangleHit> trace_closest(const Var<Ray>& ray_in) const noexcept;
    [[nodiscard]] luisa::shared_ptr<Interaction> interaction(const Var<Ray> ray, const Var<TriangleHit> hit) const noexcept;
    [[nodiscard]] luisa::shared_ptr<Interaction> intersect(const Var<Ray>& ray) const noexcept;
    [[nodiscard]] Bool intersect_any(const Var<Ray>& ray_in) const noexcept;
    [[nodiscard]] ShadingAttribute shading_point(const Shape::Handle& instance, const Var<Triangle>& triangle, const Var<float2>& bary, const Var<float4x4>& shape_to_world) const noexcept;

private:
    void process_shape(CommandBuffer& command_buffer, const Shape* shape) noexcept;
};
} // namespace Yutrel