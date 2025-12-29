#pragma once

#include "rtweekend/hittable.h"

#include <luisa/dsl/sugar.h>

namespace Yutrel::RTWeekend
{
    struct QuadData
    {
        float3 Q{make_float3(0.0f)};
        float3 u{make_float3(0.0f)};
        float3 v{make_float3(0.0f)};
        uint mat_id{0u};
    };

    class Quad : public Hittable
    {
    private:
        Float3 m_Q;
        Float3 m_u;
        Float3 m_v;
        UInt m_mat_id;
        Float3 m_normal;
        Float m_D;
        Float3 m_w;

    public:
        Quad() = delete;
        Quad(Float3 Q, Float3 u, Float3 v, UInt mat_id) noexcept
            : m_Q{Q}, m_u{u}, m_v{v}, m_mat_id{mat_id}
        {
            auto n   = cross(m_u, m_v);
            m_normal = normalize(n);
            m_D      = dot(m_normal, m_Q);
            m_w      = n / dot(n, n);
        }
        ~Quad() noexcept override = default;

        [[nodiscard]] Bool hit(Var<Ray> ray, Expr<float> t_min, Expr<float> t_max, Expr<float> time, HitRecord& rec) const noexcept override
        {
            Bool hit = true;

            auto denom = dot(m_normal, ray->direction());

            $if(abs(denom) < 1e-8f)
            {
                hit = false;
            }
            $else
            {
                auto t = (m_D - dot(m_normal, ray->origin())) / denom;
                $if(t < t_min | t_max < t)
                {
                    hit = false;
                }
                $else
                {
                    auto intersection = ray->origin() + t * ray->direction();

                    auto planar_hitpt_vec = intersection - m_Q;
                    auto alpha            = dot(m_w, cross(planar_hitpt_vec, m_v));
                    auto beta             = dot(m_w, cross(m_u, planar_hitpt_vec));

                    $if(!is_interior(alpha, beta, rec))
                    {
                        hit = false;
                    }
                    $else
                    {
                        rec.t        = t;
                        rec.position = intersection;
                        rec.mat_id   = m_mat_id;
                        rec.set_face_normal(ray, m_normal);
                    };
                };
            };

            return hit;
        }

        [[nodiscard]] Bool is_interior(Expr<float> a, Expr<float> b, HitRecord& rec) const noexcept
        {
            auto a_in01 = a >= 0.0f & a <= 1.0f;
            auto b_in01 = b >= 0.0f & b <= 1.0f;

            Bool is_interior = false;
            $if(a_in01 & b_in01)
            {
                rec.uv      = make_float2(a, b);
                is_interior = true;
            };

            return is_interior;
        }
    };
} // namespace Yutrel::RTWeekend

LUISA_STRUCT(Yutrel::RTWeekend::QuadData, Q, u, v, mat_id){};