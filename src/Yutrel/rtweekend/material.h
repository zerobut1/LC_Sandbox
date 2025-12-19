#pragma once

#include <luisa/dsl/syntax.h>

#include "rtweekend/hittable.h"
#include "utils/sampling.h"

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;
    using namespace RTWeekend;

    class Material
    {
    public:
        virtual ~Material() = default;

        virtual Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u) const noexcept
        {
            return false;
        }
    };

    class Lambertian : public Material
    {
    private:
        float3 m_albedo;

    public:
        Lambertian(const float3& a) noexcept
            : m_albedo{a} {}

        Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u) const noexcept override
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
        float3 albedo;
        float fuzz;

    public:
        Metal(const float3& a, float f) noexcept
            : albedo{a},
              fuzz{f} {}

        Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u) const noexcept override
        {
            auto reflected = reflect(ray->direction(), rec.normal);
            reflected      = normalize(reflected) + (fuzz * sample_uniform_sphere(u));

            ray         = make_ray(rec.position, reflected);
            attenuation = albedo;

            return (dot(ray->direction(), rec.normal) > 0.0f);
        }
    };

} // namespace Yutrel