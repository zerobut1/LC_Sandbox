#include "shape.h"

#include "base/scene.h"
#include "base/surface.h"
#include "shapes/mesh.h"

namespace Yutrel
{
luisa::unique_ptr<Shape> Shape::create(Scene& scene, const CreateInfo& info) noexcept
{
    switch (info.type)
    {
    case Type::mesh:
        return luisa::make_unique<Mesh>(scene, info);
    default:
        LUISA_ERROR("Unsupported shape type {}.", static_cast<uint>(info.type));
    }
}

Shape::Shape(Scene& scene, const CreateInfo& info) noexcept
    : m_surface(scene.load_surface(info.surface_info)),
      m_light(scene.load_light(info.light_info)) {}

uint4 Shape::Handle::encode(uint buffer_base, uint flags, uint surface_tag, uint light_tag) noexcept
{
    return make_uint4(buffer_base, flags, surface_tag, light_tag);
}

Shape::Handle Shape::Handle::decode(Expr<uint4> compressed) noexcept
{
    auto buffer_base = compressed.x;
    auto flags       = compressed.y;
    auto surface_tag = compressed.z;
    auto light_tag   = compressed.w;

    return Handle(buffer_base, flags, surface_tag, light_tag);
}

} // namespace Yutrel
