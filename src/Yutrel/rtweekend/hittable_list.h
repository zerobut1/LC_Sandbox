#pragma once

#include "hittable.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "rtweekend/sphere.h"

namespace Yutrel::RTWeekend
{
    class HittableList : public Hittable
    {
    private:
        BufferView<SphereData> m_sphere_buffer;
        uint m_sphere_count = 0u;

    public:
        explicit HittableList(BufferView<SphereData> sphere_buffer, uint sphere_count) noexcept
            : m_sphere_buffer{sphere_buffer}, m_sphere_count{sphere_count} {}

    public:
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
    };

} // namespace Yutrel::RTWeekend