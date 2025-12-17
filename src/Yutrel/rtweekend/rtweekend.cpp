#include "rtweekend.h"

namespace Yutrel
{
    Bool hit_sphere(Float3 center, Float radius, Var<Ray> ray) noexcept
    {
        static Callable impl = [](Float3 center, Float radius, Var<Ray> ray) noexcept -> Bool
        {
            auto direction    = normalize(ray->direction());
            auto oc           = ray->origin() - center;
            auto a            = dot(direction, direction);
            auto b            = 2.0f * dot(oc, direction);
            auto c            = dot(oc, oc) - radius * radius;
            auto discriminant = b * b - 4.0f * a * c;
            return discriminant >= 0.0f;
        };
        return impl(center, radius, ray);
    }

} // namespace Yutrel