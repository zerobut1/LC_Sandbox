#include "inline_mesh.h"

namespace Yutrel
{
InlineMesh::InlineMesh(Scene& scene, const CreateInfo& info) noexcept
    : Shape(scene, info)
{
    auto vertex_count = info.positions.size();

    if ((!info.normals.empty() && info.normals.size() != vertex_count) ||
        (!info.uvs.empty() && info.uvs.size() != vertex_count)) [[unlikely]]
    {
        LUISA_ERROR_WITH_LOCATION("Invalid inline mesh vertex attribute count.");
    }

    m_properties = (!info.uvs.empty() ? Shape::property_flag_has_vertex_uv : 0u) |
                   (!info.normals.empty() ? Shape::property_flag_has_vertex_normal : 0u);

    m_triangles.resize(info.indices.size());
    for (auto i = 0u; i < info.indices.size(); i++)
    {
        auto t = info.indices[i];
        if (t.x >= vertex_count || t.y >= vertex_count || t.z >= vertex_count) [[unlikely]]
        {
            LUISA_ERROR_WITH_LOCATION("Inline mesh triangle index out of bounds.");
        }
        m_triangles[i] = Triangle{t.x, t.y, t.z};
    }

    m_vertices.resize(vertex_count);
    for (auto i = 0u; i < vertex_count; i++)
    {
        auto p  = info.positions[i];
        auto n  = info.normals.empty() ? make_float3(0.0f, 0.0f, 1.0f) : normalize(info.normals[i]);
        auto uv = info.uvs.empty() ? make_float2(0.0f) : info.uvs[i];
        m_vertices[i] = Vertex::encode(p, n, uv);
    }
}

} // namespace Yutrel
