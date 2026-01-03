#pragma once

#include <luisa/core/stl.h>

#include "base/surface.h"
#include "utils/vertex.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Scene;

struct MeshView
{
    luisa::span<const Vertex> vertices;
    luisa::span<const Triangle> triangles;
};

class Shape
{
public:
    enum class Type
    {
        mesh,
    };

    struct CreateInfo
    {
        Type type{Type::mesh};
        // mesh
        std::filesystem::path path;

        Surface::CreateInfo surface_info;
    };

    [[nodiscard]] static luisa::unique_ptr<Shape> create(Scene& scene, const CreateInfo& info) noexcept;

public:
    class Handle;

public:
    static constexpr auto property_flag_has_vertex_normal = 1u << 0u;
    static constexpr auto property_flag_has_vertex_uv     = 1u << 1u;
    static constexpr auto property_flag_has_surface       = 1u << 2u;
    static constexpr auto property_flag_has_light         = 1u << 3u;
    static constexpr auto property_flag_has_medium        = 1u << 4u;

private:
    const Surface* m_surface;

public:
    explicit Shape(Scene& scene, const CreateInfo& info) noexcept;
    virtual ~Shape() noexcept = default;

    Shape()                        = delete;
    Shape(const Shape&)            = delete;
    Shape& operator=(const Shape&) = delete;
    Shape(Shape&&)                 = delete;
    Shape& operator=(Shape&&)      = delete;

public:
    [[nodiscard]] const Surface* surface() const noexcept { return m_surface; }

    [[nodiscard]] virtual bool is_mesh() const noexcept { return false; }
    [[nodiscard]] virtual MeshView mesh() const noexcept { return {}; }
    [[nodiscard]] virtual uint vertex_properties() const noexcept { return 0u; }
};

class Shape::Handle
{
public:
    static constexpr auto vertex_buffer_id_offset   = 0u;
    static constexpr auto triangle_buffer_id_offset = 1u;

private:
    UInt m_buffer_base;
    UInt m_properties;
    UInt m_surface_tag;

private:
    Handle(Expr<uint> buffer_base, Expr<uint> flags, Expr<uint> surface_tag) noexcept
        : m_buffer_base{buffer_base},
          m_properties{flags},
          m_surface_tag{surface_tag} {}

public:
    Handle() = delete;
    [[nodiscard]] static uint4 encode(uint buffer_base, uint flags, uint surface_tag) noexcept;
    [[nodiscard]] static Handle decode(Expr<uint4> compressed) noexcept;

public:
    [[nodiscard]] auto geometry_buffer_base() const noexcept { return m_buffer_base; }
    [[nodiscard]] auto properties() const noexcept { return m_properties; }
    [[nodiscard]] auto surface_tag() const noexcept { return m_surface_tag; }
    [[nodiscard]] auto vertex_buffer_id() const noexcept { return geometry_buffer_base() + Yutrel::Shape::Handle::vertex_buffer_id_offset; }
    [[nodiscard]] auto triangle_buffer_id() const noexcept { return geometry_buffer_base() + Yutrel::Shape::Handle::triangle_buffer_id_offset; }
    [[nodiscard]] auto test_property_flag(luisa::uint flag) const noexcept { return (properties() & flag) != 0u; }
    [[nodiscard]] auto has_vertex_normal() const noexcept { return test_property_flag(Yutrel::Shape::property_flag_has_vertex_normal); }
    [[nodiscard]] auto has_vertex_uv() const noexcept { return test_property_flag(Yutrel::Shape::property_flag_has_vertex_uv); }
    [[nodiscard]] auto has_surface() const noexcept { return test_property_flag(Yutrel::Shape::property_flag_has_surface); }
    [[nodiscard]] auto has_light() const noexcept { return test_property_flag(Yutrel::Shape::property_flag_has_light); }
    [[nodiscard]] auto has_medium() const noexcept { return test_property_flag(Yutrel::Shape::property_flag_has_medium); }
};

} // namespace Yutrel