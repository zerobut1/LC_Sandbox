#pragma once

#include "hittable.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "rtweekend/aabb.h"

namespace Yutrel::RTWeekend
{
    class HittableList : public Hittable
    {
    private:
        luisa::vector<luisa::shared_ptr<Hittable>> m_objects;
        AABB m_bbox;

    public:
        HittableList() noexcept = default;
        explicit HittableList(luisa::shared_ptr<Hittable> object) noexcept
        {
            add(std::move(object));
        }

    public:
        [[nodiscard]] auto objects() const noexcept -> const luisa::vector<luisa::shared_ptr<Hittable>>&
        {
            return m_objects;
        }

        void clear() noexcept
        {
            m_objects.clear();
        }

        void add(luisa::shared_ptr<Hittable> object) noexcept
        {
            if (m_objects.empty())
            {
                m_bbox = object->bounding_box();
            }
            else
            {
                m_bbox = AABB(m_bbox, object->bounding_box());
            }
            m_objects.push_back(std::move(object));
        }

        [[nodiscard]] Bool hit(Var<Ray> ray, Expr<float> t_min, Expr<float> t_max, Expr<float> time, HitRecord& rec) const noexcept override
        {
            Bool hit_anything    = false;
            Float closest_so_far = t_max;

            for (const auto& object : m_objects)
            {
                HitRecord temp_rec;
                $if(object->hit(ray, t_min, closest_so_far, time, temp_rec))
                {
                    hit_anything   = true;
                    closest_so_far = temp_rec.t;
                    rec            = temp_rec;
                };
            };

            return hit_anything;
        }

        [[nodiscard]] AABB bounding_box() const noexcept override
        {
            return m_bbox;
        }
    };

} // namespace Yutrel::RTWeekend