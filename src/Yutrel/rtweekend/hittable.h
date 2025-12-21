#pragma once

#include <luisa/dsl/syntax.h>

#include "rtweekend/aabb.h"

namespace Yutrel::RTWeekend
{
    using namespace luisa;
    using namespace luisa::compute;

    class HitRecord
    {
    public:
        Float3 position;
        UInt mat_id;
        Float3 normal;
        Float t;
        Bool front_face;

        void set_face_normal(Var<Ray> ray, Float3 outward_normal) noexcept
        {
            front_face = dot(ray->direction(), outward_normal) < 0.0f;
            normal     = normalize(ite(front_face, outward_normal, -outward_normal));
        }
    };

    class Hittable
    {
    public:
        Hittable() noexcept          = default;
        virtual ~Hittable() noexcept = default;

        [[nodiscard]] virtual Bool hit(Var<Ray> ray, Expr<float> t_min, Expr<float> t_max, Expr<float> time, HitRecord& rec) const noexcept = 0;

        [[nodiscard]] virtual AABB bounding_box() const noexcept = 0;
    };

} // namespace Yutrel::RTWeekend