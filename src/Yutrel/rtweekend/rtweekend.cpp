#include "rtweekend.h"

#include <luisa/luisa-compute.h>

namespace Yutrel
{
    Float hit_sphere(Float3 center, Float radius, Var<Ray> ray) noexcept
    {
        static Callable impl = [](Float3 center, Float radius, Var<Ray> ray) noexcept -> Float
        {
            auto direction    = ray->direction();
            auto oc           = center - ray->origin();
            auto a            = dot(direction, direction);
            auto b            = -2.0f * dot(direction, oc);
            auto c            = dot(oc, oc) - radius * radius;
            auto discriminant = b * b - 4.0f * a * c;

            $if(discriminant >= 0.0f)
            {
                $return((-b - sqrt(discriminant)) / (2.0f * a));
            };

            return -1.0f;
        };
        return impl(center, radius, ray);
    }

} // namespace Yutrel