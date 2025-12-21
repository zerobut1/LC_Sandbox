#pragma once

#include "hittable.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "rtweekend/aabb.h"
#include "rtweekend/sphere.h"

namespace Yutrel::RTWeekend
{
    class HittableList : public Hittable
    {
    private:
        luisa::vector<luisa::shared_ptr<Hittable>> m_objects;
        AABB m_bbox;
        BufferView<SphereData> m_sphere_buffer;
        uint m_sphere_count = 0u;

    public:
        HittableList() noexcept = default;
        explicit HittableList(luisa::shared_ptr<Hittable> object) noexcept
        {
            add(std::move(object));
        }

        explicit HittableList(BufferView<SphereData> sphere_buffer, uint sphere_count) noexcept
            : m_sphere_buffer{sphere_buffer}, m_sphere_count{sphere_count} {}

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
            Bool hit_anything = false;
            Float closest     = t_max;

            $for(i, def(m_sphere_count))
            {
                Var<SphereData> data = m_sphere_buffer->read(i);
                Sphere s(data.center, data.radius, data.velocity, data.mat_id);

                HitRecord temp_rec;
                $if(s.hit(ray, t_min, closest, time, temp_rec))
                {
                    hit_anything = true;
                    closest      = temp_rec.t;
                    rec          = temp_rec;
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