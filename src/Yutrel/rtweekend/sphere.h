#pragma once

#include "hittable.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

namespace Yutrel::RTWeekend
{
    class Sphere : public Hittable
    {
    private:
        Float3 m_center;
        Float m_radius;

    public:
        Sphere(Float3 center, Float radius) noexcept
            : m_center{center},
              m_radius{radius} {}

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
                    rec.point           = ray->origin() + ray->direction() * rec.t;
                    auto outward_normal = (rec.point - m_center) / m_radius;
                    rec.set_face_normal(ray, outward_normal);
                };
            };

            return hit;
        }
    };
} // namespace Yutrel::RTWeekend