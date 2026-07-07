#include "base/scene.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace Yutrel;

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error{message};
    }
}

void require_close(float a, float b, const char* message)
{
    if (std::abs(a - b) > 1e-5f)
    {
        throw std::runtime_error{message};
    }
}

Shape::CreateInfo make_triangle_info() noexcept
{
    Shape::CreateInfo info{
        .type = Shape::Type::inline_mesh,
    };
    info.positions = {
        make_float3(0.0f, 0.0f, 0.0f),
        make_float3(1.0f, 0.0f, 0.0f),
        make_float3(0.0f, 1.0f, 0.0f),
    };
    info.indices = {
        make_uint3(0u, 1u, 2u),
    };
    return info;
}

} // namespace

int main(int argc, char* argv[])
{
    auto context = luisa::compute::Context{argc > 0 ? argv[0] : ""};
    Scene scene{context};

    auto info = make_triangle_info();
    auto shape = Shape::create(scene, info);
    require(shape->is_mesh(), "inline mesh is mesh");
    require(!shape->surface()->two_sided(), "inline mesh default surface single-sided");
    require(shape->vertex_properties() == 0u, "inline mesh default properties");
    auto mesh = shape->mesh();
    require(mesh.vertices.size() == 3u, "inline mesh vertex count");
    require(mesh.triangles.size() == 1u, "inline mesh triangle count");
    require_close(mesh.vertices[0].normal().z, 1.0f, "inline mesh default normal");
    require_close(mesh.vertices[0].uv().x, 0.0f, "inline mesh default uv x");
    require(mesh.triangles[0].i0 == 0u && mesh.triangles[0].i1 == 1u && mesh.triangles[0].i2 == 2u,
            "inline mesh triangle indices");

    auto attributed_info = make_triangle_info();
    attributed_info.normals = {
        make_float3(0.0f, 0.0f, 2.0f),
        make_float3(0.0f, 0.0f, 2.0f),
        make_float3(0.0f, 0.0f, 2.0f),
    };
    attributed_info.uvs = {
        make_float2(0.0f, 0.0f),
        make_float2(1.0f, 0.0f),
        make_float2(0.0f, 1.0f),
    };
    auto attributed_shape = Shape::create(scene, attributed_info);
    auto properties = attributed_shape->vertex_properties();
    require((properties & Shape::property_flag_has_vertex_normal) != 0u, "inline mesh normal property");
    require((properties & Shape::property_flag_has_vertex_uv) != 0u, "inline mesh uv property");
    auto attributed_mesh = attributed_shape->mesh();
    require_close(attributed_mesh.vertices[0].normal().z, 1.0f, "inline mesh normalized normal");
    require_close(attributed_mesh.vertices[2].uv().y, 1.0f, "inline mesh uv value");

    auto two_sided_info = make_triangle_info();
    two_sided_info.surface_info.two_sided = true;
    auto two_sided_shape = Shape::create(scene, two_sided_info);
    require(two_sided_shape->surface()->two_sided(), "inline mesh explicit two-sided surface");

    std::cout << "Inline mesh tests passed.\n";
    return 0;
}
