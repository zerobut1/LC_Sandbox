#include "surface.h"

#include "surfaces/diffuse.h"
#include "surfaces/diffuselight.h"
#include "surfaces/null.h"

namespace Yutrel
{
luisa::unique_ptr<Surface> Surface::create(Scene& scene, const CreateInfo& info) noexcept
{
    switch (info.type)
    {
    case Type::diffuse:
        return luisa::make_unique<Diffuse>(scene, info);
    case Type::diffuse_light:
        return luisa::make_unique<DiffuseLight>(scene, info);
    case Type::null:
    default:
        return luisa::make_unique<NullSurface>(scene, info);
    }
}

void Surface::Instance::closure(PolymorphicCall<Closure>& call, const Interaction& it, Expr<float> time) const noexcept
{
    auto cls = call.collect(closure_identifier(), [&]
    {
        return create_closure(time);
    });
    populate_closure(cls, it);
}
} // namespace Yutrel