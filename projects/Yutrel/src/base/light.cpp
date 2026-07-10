#include "light.h"

#include "base/scene.h"
#include "lights/diffuse.h"
#include "lights/null.h"

namespace Yutrel
{
luisa::unique_ptr<Light> Light::create(Scene& scene, const CreateInfo& info) noexcept
{
    switch (info.type)
    {
    case Type::diffuse:
        return luisa::make_unique<DiffuseLight>(scene.load_texture(info.emission), info.scale, info.two_sided);
    case Type::null:
    default:
        return luisa::make_unique<NullLight>();
    }
}

} // namespace Yutrel
