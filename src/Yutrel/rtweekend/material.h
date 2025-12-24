#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "rtweekend/hittable.h"
#include "utils/sampling.h"
#include "utils/scattering.h"

namespace Yutrel::RTWeekend
{
    using namespace luisa;
    using namespace luisa::compute;
    using namespace RTWeekend;

    class Material
    {
    public:
        virtual ~Material() = default;

        virtual Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept = 0;
    };

    class Lambertian : public Material
    {
    private:
        float3 m_albedo;

    public:
        explicit Lambertian(float3 a) noexcept
            : m_albedo{a} {}

        Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
        {
            auto scatter_direction = rec.normal + sample_cosine_hemisphere(u);

            ray         = make_ray(rec.position, scatter_direction);
            attenuation = m_albedo;

            return true;
        }
    };

    class Metal : public Material
    {
    private:
        float3 m_albedo;
        float m_fuzz;

    public:
        Metal(float3 a, float f) noexcept
            : m_albedo{a},
              m_fuzz{f} {}

        Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
        {
            auto reflected = reflect(ray->direction(), rec.normal);
            reflected      = normalize(reflected) + (m_fuzz * sample_uniform_sphere(u));

            ray         = make_ray(rec.position, reflected);
            attenuation = m_albedo;

            return (dot(ray->direction(), rec.normal) > 0.0f);
        }
    };

    class Dielectric : public Material
    {
    private:
        float m_ior; // Index of Refraction
    public:
        explicit Dielectric(float ior) noexcept
            : m_ior{ior} {}

        Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
        {
            attenuation = make_float3(1.0f);
            auto eta    = ite(rec.front_face, 1.0f / m_ior, m_ior);

            auto unit_direction  = normalize(ray->direction());
            Float3 out_direction = make_float3(0.0f);
            auto cos_theta       = saturate(dot(-unit_direction, rec.normal));
            auto r0              = (1.0f - eta) / (1.0f + eta);
            r0                   = r0 * r0;

            $if(u_lobe < FrSchlick(r0, cos_theta))
            {
                out_direction = reflect(unit_direction, rec.normal);
            }
            $else
            {
                auto refracted = refract(-unit_direction, rec.normal, eta, &out_direction);
                $if(!refracted)
                {
                    out_direction = reflect(unit_direction, rec.normal);
                };
            };

            ray = make_ray(rec.position, out_direction);

            return true;
        }

        [[nodiscard]] static Float SchlickWeight(Expr<float> cosTheta) noexcept
        {
            auto m = saturate(1.f - cosTheta);
            return sqr(sqr(m)) * m;
        }

        [[nodiscard]] static Float FrSchlick(Expr<float> R0, Expr<float> cosTheta) noexcept
        {
            return lerp(R0, 1.f, SchlickWeight(cosTheta));
        }
    };

} // namespace Yutrel::RTWeekend
