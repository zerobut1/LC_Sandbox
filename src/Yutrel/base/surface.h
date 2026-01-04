#pragma once

#include <luisa/core/stl.h>
#include <luisa/dsl/syntax.h>

#include "base/texture.h"
#include "utils/command_buffer.h"
#include "utils/polymorphic_closure.h"

namespace Yutrel
{
class Scene;
class Renderer;
class Interaction;

class Surface
{
public:
    enum class Type
    {
        null,
        diffuse,
    };

    struct CreateInfo
    {
        Type type{Type::null};
        // diffuse
        Texture::CreateInfo reflectance{};
    };

    [[nodiscard]] static luisa::unique_ptr<Surface> create(Scene& scene, const CreateInfo& info) noexcept;

public:
    static constexpr auto event_reflect  = 0x00u;
    static constexpr auto event_enter    = 0x01u;
    static constexpr auto event_exit     = 0x02u;
    static constexpr auto event_transmit = event_enter | event_exit;
    static constexpr auto event_through  = 0x04u;

    struct Evaluation
    {
        Float3 f;
        Float pdf;
        Float3 f_diffuse;
        Float pdf_diffuse;
        [[nodiscard]] static auto zero() noexcept
        {
            return Evaluation{
                .f           = make_float3(0.f),
                .pdf         = 0.f,
                .f_diffuse   = make_float3(0.f),
                .pdf_diffuse = 0.f};
        }
    };

    struct Sample
    {
        Evaluation eval;
        Float3 wi;
        UInt event;

        [[nodiscard]] static auto zero() noexcept
        {
            return Sample{
                .eval  = Evaluation::zero(),
                .wi    = make_float3(0.f, 0.f, 1.f),
                .event = Surface::event_reflect};
        }
    };

public:
    class Instance;
    class Closure;

public:
    explicit Surface(Scene& scene, const CreateInfo& info) noexcept {}
    virtual ~Surface() noexcept = default;

    Surface()                          = delete;
    Surface(const Surface&)            = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&&)                 = delete;
    Surface& operator=(Surface&&)      = delete;

public:
    [[nodiscard]] virtual bool is_null() const noexcept { return false; }
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept = 0;
};

class Surface::Instance
{
private:
    const Renderer& m_renderer;
    const Surface* m_surface;

public:
    explicit Instance(const Renderer& renderer, const Surface* surface) noexcept
        : m_renderer{renderer}, m_surface{surface} {}
    virtual ~Instance() noexcept = default;

    Instance()                           = delete;
    Instance(const Instance&)            = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&)                 = delete;
    Instance& operator=(Instance&&)      = delete;

public:
    template <typename T = Surface>
        requires std::is_base_of_v<Surface, T>
    [[nodiscard]] auto base() const noexcept
    {
        return static_cast<const T*>(m_surface);
    }
    [[nodiscard]] auto& renderer() const noexcept { return m_renderer; }
    void closure(PolymorphicCall<Closure>& call, const Interaction& it, Expr<float> time) const noexcept;

    [[nodiscard]] virtual luisa::string closure_identifier() const noexcept                          = 0;
    [[nodiscard]] virtual luisa::unique_ptr<Closure> create_closure(Expr<float> time) const noexcept = 0;
    virtual void populate_closure(Closure* closure, const Interaction& it) const noexcept            = 0;
};

class Surface::Closure : public PolymorphicClosure
{
private:
    const Renderer& m_renderer;
    Float m_time;

public:
    explicit Closure(const Renderer& renderer, Float time) noexcept
        : m_renderer{renderer}, m_time{time} {}
    virtual ~Closure() noexcept override = default;

    Closure()                          = delete;
    Closure(const Closure&)            = delete;
    Closure& operator=(const Closure&) = delete;
    Closure(Closure&&)                 = delete;
    Closure& operator=(Closure&&)      = delete;

public:
    [[nodiscard]] auto& renderer() const noexcept { return m_renderer; }
    [[nodiscard]] auto time() const noexcept { return m_time; }
    [[nodiscard]] virtual const Interaction& it() const noexcept = 0;

    [[nodiscard]] Surface::Sample sample(Expr<float3> wo, Expr<float> u_lobe, Expr<float2> u) const noexcept;
    [[nodiscard]] Surface::Evaluation evaluate(Expr<float3> wo, Expr<float3> wi) const noexcept;

private:
    [[nodiscard]] virtual Surface::Sample sample_impl(Expr<float3> wo, Expr<float> u_lobe, Expr<float2> u) const noexcept = 0;
    [[nodiscard]] virtual Surface::Evaluation evaluate_impl(Expr<float3> wo, Expr<float3> wi) const noexcept = 0;
};
} // namespace Yutrel