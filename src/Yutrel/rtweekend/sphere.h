#pragma once

#include "hittable.h"

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "rtweekend/aabb.h"

namespace Yutrel::RTWeekend
{
    struct SphereData
    {
        float3 center{make_float3(0.0f)};
        float radius{0.0f};
        float3 velocity{make_float3(0.0f)};
        uint mat_id{0u};
    };

    class Sphere : public Hittable
    {
    private:
        Float3 m_center;
        Float m_radius;
        Float3 m_velocity{make_float3(0.0f)};
        UInt m_mat_id;
        AABB m_bbox;

    public:
        Sphere(Float3 center, Float radius, UInt mat_id) noexcept
            : m_center{center},
              m_radius{radius},
              m_mat_id{mat_id} {}

        Sphere(float3 center, float radius, uint mat_id) noexcept
            : Sphere(def(center), radius, mat_id)
        {
            float3 radius_vec = make_float3(radius);
            m_bbox            = AABB(center - radius_vec, center + radius_vec);
        }

        Sphere(Float3 center, Float radius, Float3 velocity, UInt mat_id) noexcept
            : m_center{center},
              m_radius{radius},
              m_mat_id{mat_id},
              m_velocity{velocity} {}

        Sphere(float3 center, float radius, float3 velocity, uint mat_id) noexcept
            : Sphere(def(center), radius, def(velocity), mat_id)
        {
            // 假设运动时间限定在0-1之间
            float3 radius_vec = make_float3(radius);
            AABB box0(center - radius_vec, center + radius_vec);
            AABB box1(center + velocity - radius_vec, center + velocity + radius_vec);
            m_bbox = AABB(box0, box1);
        }

        [[nodiscard]] Bool hit(Var<Ray> ray, Expr<float> t_min, Expr<float> t_max, Expr<float> time, HitRecord& rec) const noexcept override
        {
            Bool hit = true;

            auto current_center = m_center + m_velocity * time;
            auto oc             = current_center - ray->origin();
            auto a              = dot(ray->direction(), ray->direction());
            auto h              = dot(ray->direction(), oc);
            auto c              = dot(oc, oc) - m_radius * m_radius;
            auto discriminant   = h * h - a * c;

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
                    auto outward_normal = (rec.position - current_center) / m_radius;
                    rec.set_face_normal(ray, outward_normal);
                    rec.mat_id = m_mat_id;
                };
            };

            return hit;
        }

        [[nodiscard]] AABB bounding_box() const noexcept override
        {
            return m_bbox;
        }
    };
} // namespace Yutrel::RTWeekend

LUISA_STRUCT(Yutrel::RTWeekend::SphereData, center, radius, velocity, mat_id){};