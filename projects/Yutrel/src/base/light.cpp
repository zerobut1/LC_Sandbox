#include "light.h"

#include "lights/diffuse.h"
#include "lights/null.h"

namespace Yutrel
{
luisa::unique_ptr<Light> Light::create(Scene& scene, const CreateInfo& info) noexcept
{
    switch (info.type)
    {
    case Type::diffuse:
        return luisa::make_unique<DiffuseLight>(scene, info);
    case Type::null:
    default:
        return luisa::make_unique<NullLight>(scene, info);
    }
}

} // namespace Yutrel