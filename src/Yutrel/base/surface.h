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
        diffuse,
        diffuse_light,
    };

    struct CreateInfo
    {
        Type type{Type::diffuse};
        // diffuse
        Texture::CreateInfo reflectance{};
        // diffuse light
        Texture::CreateInfo emit{};
    };

    [[nodiscard]] static luisa::unique_ptr<Surface> create(Scene& scene, const CreateInfo& info) noexcept;

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

    [[nodiscard]] virtual Bool scatter(Var<Ray>& ray, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept = 0;
    [[nodiscard]] virtual Float scatter_pdf(Expr<float3> wo, Expr<float3> wi, Var<float3>& attenuation, Expr<float2> u, Expr<float> u_lobe) const noexcept { return 0; }
    [[nodiscard]] virtual Float3 emitted() const noexcept { return make_float3(0.0f); }
};

} // namespace Yutrel