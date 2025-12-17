#pragma once

#include "hittable.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

namespace Yutrel::RTWeekend
{
    class HittableList : public Hittable
    {
    public:
        luisa::vector<luisa::shared_ptr<Hittable>> objects;

    public:
        HittableList() noexcept = default;
        explicit HittableList(luisa::shared_ptr<Hittable> object) noexcept
        {
            add(object);
        }

    public:
        void clear() noexcept
        {
            objects.clear();
        }

        void add(luisa::shared_ptr<Hittable> object) noexcept
        {
            objects.push_back(object);
        }

        Bool hit(Var<Ray> ray, Float t_min, Float t_max, HitRecord& rec) const noexcept override
        {
            Bool hit_anything    = false;
            Float closest_so_far = t_max;

            for (const auto& object : objects)
            {
                HitRecord temp_rec;
                $if(object->hit(ray, t_min, closest_so_far, temp_rec))
                {
                    hit_anything   = true;
                    closest_so_far = temp_rec.t;
                    rec            = temp_rec;
                };
            };

            return hit_anything;
        }
    };

} // namespace Yutrel::RTWeekend