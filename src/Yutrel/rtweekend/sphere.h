#pragma once

#include "hittable.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

namespace Yutrel::RTWeekend
{
    class Sphere : public Hittable
    {
    private:
        float3 m_center;
        float m_radius;
        uint m_mat_id;

    public:
        Sphere(float3 center, float radius, uint mat_id) noexcept
            : m_center{center},
              m_radius{radius},
              m_mat_id{mat_id} {}

        [[nodiscard]] Bool hit(Var<Ray> ray, Float t_min, Float t_max, HitRecord& rec) const noexcept override
        {
            Bool hit = true;

            auto oc           = m_center - ray->origin();
            auto a            = dot(ray->direction(), ray->direction());
            auto h            = dot(ray->direction(), oc);
            auto c            = dot(oc, oc) - m_radius * m_radius;
            auto discriminant = h * h - a * c;

            $if(discriminant < 0.0f)
            {
                hit = false;
            }
            $else
            {
                auto sqrt_d = sqrt(discriminant);
                auto root   = (h - sqrt_d) / a;
                $if(root <= t_min | t_max <= root)
                {
                    root = (h + sqrt_d) / a;
                    $if(root <= t_min | t_max <= root)
                    {
                        hit = false;
                    };
                };

                $if(hit)
                {
                    rec.t               = root;
                    rec.position        = ray->origin() + ray->direction() * rec.t;
                    auto outward_normal = (rec.position - m_center) / m_radius;
                    rec.set_face_normal(ray, outward_normal);
                    rec.mat_id = m_mat_id;
                };
            };

            return hit;
        }
    };
} // namespace Yutrel::RTWeekend