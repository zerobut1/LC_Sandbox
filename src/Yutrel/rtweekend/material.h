#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

#include "base/scene.h"
#include "base/texture.h"
#include "rtweekend/hittable.h"
#include "utils/command_buffer.h"
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
            [[nodiscard]] virtual Bool scatter(Var<Ray>& ray, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept = 0;
            [[nodiscard]] virtual Float3 emitted() const noexcept
            {
                return make_float3(0.0f);
            }
        };

        class Instance
        {
        private:
            const Renderer& m_renderer;
            const Material* m_material;

        public:
            explicit Instance(const Renderer& renderer, const Material* material) noexcept
                : m_renderer(renderer), m_material(material) {}
            virtual ~Instance() noexcept = default;

        public:
            void closure(PolymorphicCall<Closure>& call, const HitRecord& rec) const noexcept
            {
                auto cls = call.collect(closure_identifier(), [&]
                {
                    return create_closure();
                });
                populate_closure(cls, rec);
            }

            [[nodiscard]] virtual luisa::string closure_identifier() const noexcept              = 0;
            [[nodiscard]] virtual luisa::unique_ptr<Closure> create_closure() const noexcept     = 0;
            virtual void populate_closure(Closure* closure, const HitRecord& rec) const noexcept = 0;
        };

    public:
        virtual ~Material() = default;

        [[nodiscard]] virtual luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept = 0;
    };

    class Lambertian : public Material
    {
    public:
        class Closure : public Material::Closure
        {
        public:
            struct Context
            {
                HitRecord rec;
                Float3 albedo;
            };

            [[nodiscard]] Bool scatter(Var<Ray>& ray, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
            {
                auto&& ctx             = context<Context>();
                auto scatter_direction = normalize(ctx.rec.normal + sample_uniform_sphere(u));

                ray         = make_ray(ctx.rec.position, scatter_direction);
                attenuation = ctx.albedo;

                return true;
            }
        };

        class Instance : public Material::Instance
        {
        private:
            const Texture::Instance* m_albedo;

        public:
            explicit Instance(const Renderer& renderer, const Material* material, const Texture::Instance* albedo) noexcept
                : Material::Instance(renderer, material), m_albedo(albedo) {}
            ~Instance() noexcept override = default;

        public:
            [[nodiscard]] luisa::string closure_identifier() const noexcept override
            {
                return "Lambertian";
            }

            [[nodiscard]] luisa::unique_ptr<Material::Closure> create_closure() const noexcept override
            {
                return luisa::make_unique<Closure>();
            }

            void populate_closure(Material::Closure* closure, const HitRecord& rec) const noexcept override
            {
                Float3 albedo = m_albedo->evaluate(rec).xyz();

                Lambertian::Closure::Context ctx{
                    .rec    = rec,
                    .albedo = albedo,
                };
                closure->bind(std::move(ctx));
            }
        };

    private:
        const Texture* m_albedo;

    public:
        explicit Lambertian(Scene& scene, const Texture::CreateInfo& info) noexcept
        {
            m_albedo = scene.load_texture(info);
        }

        [[nodiscard]] luisa::unique_ptr<Material::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
    };

    class Metal : public Material
    {
    public:
        class Closure : public Material::Closure
        {
        public:
            struct Context
            {
                HitRecord rec;
                Float3 albedo;
                Float fuzz;
            };

            [[nodiscard]] Bool scatter(Var<Ray>& ray, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
            {
                auto&& ctx     = context<Context>();
                auto normal    = ctx.rec.normal;
                auto reflected = reflect(ray->direction(), normal);
                reflected      = normalize(reflected) + (ctx.fuzz * sample_uniform_sphere(u));

                ray         = make_ray(ctx.rec.position, reflected);
                attenuation = ctx.albedo;

                return (dot(ray->direction(), normal) > 0.0f);
            }
        };

        class Instance : public Material::Instance
        {
        private:
            const Texture::Instance* m_albedo_fuzz;

        public:
            explicit Instance(const Renderer& renderer, const Material* material, const Texture::Instance* albedo_fuzz) noexcept
                : Material::Instance(renderer, material), m_albedo_fuzz(albedo_fuzz) {}
            ~Instance() noexcept override = default;

        public:
            [[nodiscard]] luisa::string closure_identifier() const noexcept override
            {
                return "Metal";
            }

            [[nodiscard]] luisa::unique_ptr<Material::Closure> create_closure() const noexcept override
            {
                return luisa::make_unique<Closure>();
            }

            void populate_closure(Material::Closure* closure, const HitRecord& rec) const noexcept override
            {
                auto albedo_fuzz = m_albedo_fuzz->evaluate(rec);
                auto albedo      = albedo_fuzz.xyz();
                auto fuzz        = albedo_fuzz.w;

                Metal::Closure::Context ctx{
                    .rec    = rec,
                    .albedo = albedo,
                    .fuzz   = fuzz,
                };
                closure->bind(std::move(ctx));
            }
        };

    private:
        const Texture* m_albedo_fuzz;

    public:
        Metal(Scene& scene, float3 a, float f) noexcept
        {
            Texture::CreateInfo info{

                .type = Texture::Type::constant,
                .v    = make_float4(a, f),
            };

            m_albedo_fuzz = scene.load_texture(info);
        }

        [[nodiscard]] luisa::unique_ptr<Material::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
    };

    class Dielectric : public Material
    {
    public:
        class Closure : public Material::Closure
        {
        public:
            struct Context
            {
                HitRecord rec;
                Float ior;
            };

            [[nodiscard]] Bool scatter(Var<Ray>& ray, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
            {
                auto&& ctx  = context<Context>();
                auto normal = ctx.rec.normal;
                attenuation = make_float3(1.0f);
                auto eta    = ite(ctx.rec.front_face, 1.0f / ctx.ior, ctx.ior);

                auto unit_direction  = normalize(ray->direction());
                Float3 out_direction = make_float3(0.0f);
                auto cos_theta       = saturate(dot(-unit_direction, normal));
                auto r0              = (1.0f - eta) / (1.0f + eta);
                r0                   = r0 * r0;

                $if(u_lobe < FrSchlick(r0, cos_theta))
                {
                    out_direction = reflect(unit_direction, normal);
                }
                $else
                {
                    auto refracted = refract(-unit_direction, normal, eta, &out_direction);
                    $if(!refracted)
                    {
                        out_direction = reflect(unit_direction, normal);
                    };
                };

                ray = make_ray(ctx.rec.position, out_direction);
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

        class Instance : public Material::Instance
        {
        private:
            float m_ior;

        public:
            explicit Instance(const Renderer& renderer, const Material* material, float ior) noexcept
                : Material::Instance(renderer, material), m_ior(ior) {}
            ~Instance() noexcept override = default;

        public:
            [[nodiscard]] luisa::string closure_identifier() const noexcept override
            {
                return "Dielectric";
            }

            [[nodiscard]] luisa::unique_ptr<Material::Closure> create_closure() const noexcept override
            {
                return luisa::make_unique<Closure>();
            }

            void populate_closure(Material::Closure* closure, const HitRecord& rec) const noexcept override
            {
                auto ior = m_ior;

                Dielectric::Closure::Context ctx{
                    .rec = rec,
                    .ior = ior,
                };
                closure->bind(std::move(ctx));
            }
        };

    private:
        float m_ior; // Index of Refraction

    public:
        explicit Dielectric(float ior) noexcept
            : m_ior{ior} {}

        [[nodiscard]] luisa::unique_ptr<Material::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
    };

    class DiffuseLight : public Material
    {
    public:
        class Closure : public Material::Closure
        {
        public:
            struct Context
            {
                HitRecord rec;
                Float3 emit;
            };

            [[nodiscard]] Bool scatter(Var<Ray>& ray, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept override
            {
                return false;
            }

            [[nodiscard]] Float3 emitted() const noexcept override
            {
                auto&& ctx = context<Context>();
                return ctx.emit;
            }
        };

        class Instance : public Material::Instance
        {
        private:
            const Texture::Instance* m_emit;

        public:
            explicit Instance(const Renderer& renderer, const Material* material, const Texture::Instance* emit) noexcept
                : Material::Instance(renderer, material), m_emit(emit) {}
            ~Instance() noexcept override = default;

        public:
            [[nodiscard]] luisa::string closure_identifier() const noexcept override
            {
                return "DiffuseLight";
            }

            [[nodiscard]] luisa::unique_ptr<Material::Closure> create_closure() const noexcept override
            {
                return luisa::make_unique<Closure>();
            }

            void populate_closure(Material::Closure* closure, const HitRecord& rec) const noexcept override
            {
                Float3 emit = m_emit->evaluate(rec).xyz();

                DiffuseLight::Closure::Context ctx{
                    .rec  = rec,
                    .emit = emit,
                };
                closure->bind(std::move(ctx));
            }
        };

    private:
        const Texture* m_emit;

    public:
        explicit DiffuseLight(Scene& scene, const Texture::CreateInfo& info) noexcept
        {
            m_emit = scene.load_texture(info);
        }

        [[nodiscard]] luisa::unique_ptr<Material::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
    };

} // namespace Yutrel::RTWeekend
