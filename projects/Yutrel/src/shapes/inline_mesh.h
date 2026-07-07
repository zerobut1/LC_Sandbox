#pragma once

#include "base/shape.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class InlineMesh final : public Shape
{
private:
    luisa::vector<Vertex> m_vertices;
    luisa::vector<Triangle> m_triangles;
    uint m_properties{};

public:
    explicit InlineMesh(Scene& scene, const CreateInfo& info) noexcept;
    ~InlineMesh() noexcept override = default;

public:
    [[nodiscard]] bool is_mesh() const noexcept override { return true; }
    [[nodiscard]] MeshView mesh() const noexcept override { return {m_vertices, m_triangles}; }
    [[nodiscard]] uint vertex_properties() const noexcept override { return m_properties; }
};

} // namespace Yutrel
