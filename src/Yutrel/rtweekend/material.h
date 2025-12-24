#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "rtweekend/hittable.h"
#include "utils/polymorphic_closure.h"
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
        class Closure : public PolymorphicClosure
        {
        public:
            [[nodiscard]] virtual Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept = 0;
        };

    public:
        virtual ~Material() = default;

        void closure(PolymorphicCall<Closure>& call) const noexcept
        {
            auto cls = call.collect(closure_identifier(), [&]
            {
                return create_closure();
            });
            populate_closure(cls);
        }

        virtual luisa::string closure_identifier() const noexcept                        = 0;
        [[nodiscard]] virtual luisa::unique_ptr<Closure> create_closure() const noexcept = 0;
        virtual void populate_closure(Closure* closure) const noexcept                   = 0;
    };

    class Lambertian : public Material
    {
    private:
        float3 m_albedo;

    public:
        class Closure : public Material::Closure
        {
        public:
            struct Context
            {
                Float3 albedo;
            };

            [[nodiscard]] Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
            {
                auto&& ctx             = context<Context>();
                auto scatter_direction = rec.normal + sample_cosine_hemisphere(u);

                ray         = make_ray(rec.position, scatter_direction);
                attenuation = ctx.albedo;

                return true;
            }
        };

    public:
        explicit Lambertian(float3 a) noexcept
            : m_albedo{a} {}

        luisa::string closure_identifier() const noexcept override
        {
            return "Lambertian";
        }

        [[nodiscard]] luisa::unique_ptr<Material::Closure> create_closure() const noexcept override
        {
            return luisa::make_unique<Closure>();
        }

        void populate_closure(Material::Closure* closure) const noexcept override
        {
            auto albedo = m_albedo;

            Lambertian::Closure::Context ctx{
                .albedo = albedo,
            };
            closure->bind(std::move(ctx));
        }
    };

    class Metal : public Material
    {
    public:
        class Closure : public Material::Closure
        {
        public:
            struct Context
            {
                Float3 albedo;
                Float fuzz;
            };

            [[nodiscard]] Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
            {
                auto&& ctx     = context<Context>();
                auto reflected = reflect(ray->direction(), rec.normal);
                reflected      = normalize(reflected) + (ctx.fuzz * sample_uniform_sphere(u));

                ray         = make_ray(rec.position, reflected);
                attenuation = ctx.albedo;

                return (dot(ray->direction(), rec.normal) > 0.0f);
            }
        };

    private:
        float3 m_albedo;
        float m_fuzz;

    public:
        Metal(float3 a, float f) noexcept
            : m_albedo{a},
              m_fuzz{f} {}

        luisa::string closure_identifier() const noexcept override
        {
            return "Metal";
        }

        [[nodiscard]] luisa::unique_ptr<Material::Closure> create_closure() const noexcept override
        {
            return luisa::make_unique<Closure>();
        }

        void populate_closure(Material::Closure* closure) const noexcept override
        {
            auto albedo = m_albedo;
            auto fuzz   = m_fuzz;

            Metal::Closure::Context ctx{
                .albedo = albedo,
                .fuzz   = fuzz,
            };
            closure->bind(std::move(ctx));
        }
    };

    class Dielectric : public Material
    {
    public:
        class Closure : public Material::Closure
        {
        public:
            struct Context
            {
                float ior;
            };

            [[nodiscard]] Bool scatter(Var<Ray>& ray, const HitRecord& rec, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
            {
                auto&& ctx  = context<Context>();
                attenuation = make_float3(1.0f);
                auto eta    = ite(rec.front_face, 1.0f / ctx.ior, ctx.ior);

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

    private:
        float m_ior; // Index of Refraction

    public:
        explicit Dielectric(float ior) noexcept
            : m_ior{ior} {}

        luisa::string closure_identifier() const noexcept override
        {
            return "Dielectric";
        }

        [[nodiscard]] luisa::unique_ptr<Material::Closure> create_closure() const noexcept override
        {
            return luisa::make_unique<Closure>();
        }

        void populate_closure(Material::Closure* closure) const noexcept override
        {
            auto ior = m_ior;

            Dielectric::Closure::Context ctx{
                .ior = ior,
            };
            closure->bind(std::move(ctx));
        }
    };

} // namespace Yutrel::RTWeekend
