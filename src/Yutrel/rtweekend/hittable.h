#pragma once

#include <luisa/dsl/syntax.h>

namespace Yutrel::RTWeekend
{
    using namespace luisa;
    using namespace luisa::compute;

    class HitRecord
    {
    public:
        Float3 point;
        Float3 normal;
        Float t;
        Bool front_face;

        void set_face_normal(Var<Ray> ray, Float3 outward_normal) noexcept
        {
            front_face = dot(ray->direction(), outward_normal) < 0.0f;
            normal     = select(-outward_normal, outward_normal, front_face);
        }
    };

    class Hittable
    {
    public:
        Hittable() noexcept          = default;
        virtual ~Hittable() noexcept = default;

        [[nodiscard]] virtual Bool hit(Var<Ray> ray, Float t_min, Float t_max, HitRecord& rec) const noexcept = 0;
    };

} // namespace Yutrel::RTWeekend