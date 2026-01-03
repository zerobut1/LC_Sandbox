#include "geometry.h"

#include "base/interaction.h"
#include "base/renderer.h"

namespace Yutrel
{
void Geometry::build(CommandBuffer& command_buffer, luisa::span<const Shape* const> shapes) noexcept
{
    m_accel = m_renderer.device().create_accel({});

    for (auto shape : shapes)
    {
        process_shape(command_buffer, shape);
    }
    LUISA_INFO_WITH_LOCATION("Geometry built with {} unique triangles ({} instanced).",
                             m_triangle_count,
                             m_instanced_triangle_count);

    m_instance_buffer = m_renderer.device().create_buffer<uint4>(m_instances.size());
    command_buffer
        << m_instance_buffer.copy_from(m_instances.data())
        << m_accel.build()
        << commit();
}

void Geometry::process_shape(CommandBuffer& command_buffer, const Shape* shape) noexcept
{
    auto surface = shape->surface();
    auto light   = shape->light();

    if (shape->is_mesh())
    {
        auto mesh = [&]
        {
            if (auto it = m_meshes.find(shape); it != m_meshes.end())
            {
                return it->second;
            }
            auto mesh_gemo = [&]
            {
                auto [vertices, triangles] = shape->mesh();
                LUISA_ASSERT(!vertices.empty() && !triangles.empty(), "Empty mesh.");
                auto hash = luisa::hash64(vertices.data(), vertices.size_bytes(), luisa::hash64_default_seed);
                hash      = luisa::hash64(triangles.data(), triangles.size_bytes(), hash);
                if (auto mesh_it = m_mesh_cache.find(hash); mesh_it != m_mesh_cache.end())
                {
                    return mesh_it->second;
                }
                // create mesh
                m_triangle_count += triangles.size();
                auto vertex_buffer   = m_renderer.create<Buffer<Vertex>>(vertices.size());
                auto triangle_buffer = m_renderer.create<Buffer<Triangle>>(triangles.size());
                auto mesh            = m_renderer.create<compute::Mesh>(*vertex_buffer, *triangle_buffer, AccelOption{});
                command_buffer
                    << vertex_buffer->copy_from(vertices.data())
                    << triangle_buffer->copy_from(triangles.data())
                    << commit()
                    << mesh->build()
                    << commit();
                auto vertex_buffer_id   = m_renderer.register_bindless(vertex_buffer->view());
                auto triangle_buffer_id = m_renderer.register_bindless(triangle_buffer->view());

                auto geom = MeshGeometry{
                    .resource       = mesh,
                    .buffer_id_base = vertex_buffer_id};
                m_mesh_cache.emplace(hash, geom);
                return geom;
            }();

            auto encode_fixed_point = [](float x) noexcept
            {
                return static_cast<uint16_t>(std::clamp(
                    std::round(x * 65535.f),
                    0.f,
                    65535.f));
            };

            MeshData data{
                .resource                = mesh_gemo.resource,
                .geometry_buffer_id_base = mesh_gemo.buffer_id_base,
                .vertex_properties       = shape->vertex_properties()};
            m_meshes.emplace(shape, data);
            return data;
        }();

        // surfaces
        auto surface_tag = 0u;
        auto properties  = mesh.vertex_properties;
        if (surface && !surface->is_null())
        {
            surface_tag = m_renderer.register_surface(command_buffer, surface);
            properties |= Shape::property_flag_has_surface;
        }

        m_accel.emplace_back(*mesh.resource, make_float4x4(1.0f));

        // lights
        auto light_tag = 0u;
        if (light && !light->is_null())
        {
            light_tag = m_renderer.register_light(command_buffer, light);
            properties |= Shape::property_flag_has_light;
        }

        m_instances.emplace_back(
            Shape::Handle::encode(
                mesh.geometry_buffer_id_base,
                properties,
                surface_tag,
                light_tag));

        m_instanced_triangle_count += mesh.resource->triangle_count();
    }
}

Var<TriangleHit> Geometry::trace_closest(const Var<Ray>& ray_in) const noexcept
{
    auto hit = m_accel->intersect(ray_in, {});

    return hit;
}

luisa::shared_ptr<Interaction> Geometry::intersect(const Var<Ray>& ray) const noexcept
{
    return interaction(ray, trace_closest(ray));
}

luisa::shared_ptr<Interaction> Geometry::interaction(const Var<Ray> ray, const Var<TriangleHit> hit) const noexcept
{
    auto encoded_shape = def(make_uint4(0u));
    auto p_g           = def(make_float3(0.0f));
    auto n_g           = def(make_float3(0.0f, 1.0f, 0.0f));
    auto uv            = def(make_float2(0.0f));
    auto area          = def(0.0f);
    auto front_face    = def(false);
    auto valid         = def(false);
    Frame shading;

    $if(!hit->miss())
    {
        valid         = true;
        encoded_shape = m_instance_buffer->read(hit.inst);
        auto shape    = Shape::Handle::decode(encoded_shape);

        auto local_to_world = m_accel->instance_transform(hit.inst);
        auto triangle       = m_renderer.buffer<Triangle>(shape.triangle_buffer_id()).read(hit.prim);
        auto v_buffer       = shape.vertex_buffer_id();

        auto v0 = m_renderer.buffer<Vertex>(v_buffer).read(triangle.i0);
        auto v1 = m_renderer.buffer<Vertex>(v_buffer).read(triangle.i1);
        auto v2 = m_renderer.buffer<Vertex>(v_buffer).read(triangle.i2);

        // local space
        auto p0_local = v0->position();
        auto p1_local = v1->position();
        auto p2_local = v2->position();
        auto n_local  = triangle_interpolate(hit.bary, v0->normal(), v1->normal(), v2->normal());

        auto uv0        = v0->uv();
        auto uv1        = v1->uv();
        auto uv2        = v2->uv();
        auto duv0       = uv1 - uv0;
        auto duv1       = uv2 - uv0;
        auto det        = duv0.x * duv1.y - duv0.y * duv1.x;
        auto inv_det    = 1.f / det;
        auto dp0_local  = p1_local - p0_local;
        auto dp1_local  = p2_local - p0_local;
        auto dpdu_local = (dp0_local * duv1.y - dp1_local * duv0.y) * inv_det;
        auto dpdv_local = (dp1_local * duv0.x - dp0_local * duv1.x) * inv_det;

        // world space
        auto m              = make_float3x3(local_to_world);
        auto t              = make_float3(local_to_world[3]);
        p_g                 = m * triangle_interpolate(hit.bary, p0_local, p1_local, p2_local) + t;
        auto c              = cross(m * dp0_local, m * dp1_local);
        area                = length(c) * 0.5f;
        n_g                 = normalize(c);
        auto fallback_frame = Frame::make(n_g);
        auto dpdu           = ite(det == 0.f, fallback_frame.s(), m * dpdu_local);
        auto dpdv           = ite(det == 0.f, fallback_frame.t(), m * dpdv_local);
        auto mn             = transpose(inverse(m));
        auto n_s            = ite(shape.has_vertex_normal(), normalize(mn * n_local), n_g);
        n_s                 = ite(dot(n_s, n_g) < 0.f, -n_s, n_s);
        shading             = Frame::make(n_s, dpdu);
        uv                  = ite(shape.has_vertex_uv(), triangle_interpolate(hit.bary, uv0, uv1, uv2), hit.bary);
        front_face          = dot(-ray->direction(), n_g) > 0.0f;
    };

    auto shape = Shape::Handle::decode(encoded_shape);
    Interaction it{
        .shape      = shape,
        .p_g        = p_g,
        .n_g        = n_g,
        .uv         = uv,
        .p_s        = p_g, // TODO: apply normal offset
        .shading    = shading,
        .inst_id    = hit.inst,
        .prim_id    = hit.prim,
        .prim_area  = area,
        .front_face = front_face,
    };
    return luisa::make_shared<Interaction>(std::move(it));
}

} // namespace Yutrel