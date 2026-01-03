#pragma once

#include <luisa/dsl/syntax.h>

#include "base/texture.h"
#include "utils/command_buffer.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Scene;
class Renderer;

class Light
{
public:
    struct Handle
    {
        uint instance_id;
        uint light_tag;
    };

    struct Evaluation
    {
        Float3 L;
        Float pdf;
        Float3 p;
        Float3 ng;
        [[nodiscard]] static auto zero() noexcept
        {
            return Evaluation{
                .L   = make_float3(0.0f),
                .pdf = 0.0f,
                .p   = make_float3(0.0f),
                .ng  = make_float3(0.0f),
            };
        }
    };

public:
    class Instance;
    class Closure;

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
        Texture::CreateInfo emission{};
        float scale{1.0f};
        bool two_sided{false};
    };

    [[nodiscard]] static luisa::unique_ptr<Light> create(Scene& scene, const CreateInfo& info) noexcept;

public:
    explicit Light(Scene& scene, const CreateInfo& info) noexcept {}
    virtual ~Light() noexcept = default;

    Light()                        = delete;
    Light(const Light&)            = delete;
    Light& operator=(const Light&) = delete;
    Light(Light&&)                 = delete;
    Light& operator=(Light&&)      = delete;

public:
    [[nodiscard]] virtual bool is_null() const noexcept { return false; }
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept = 0;
};

class Light::Instance
{
private:
    const Renderer& m_renderer;
    const Light* m_light;

public:
    explicit Instance(const Renderer& renderer, const Light* light) noexcept
        : m_renderer{renderer}, m_light{light} {}
    virtual ~Instance() noexcept = default;

    Instance()                           = delete;
    Instance(const Instance&)            = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&)                 = delete;
    Instance& operator=(Instance&&)      = delete;

public:
    template <typename T = Light>
        requires std::is_base_of_v<Light, T>
    [[nodiscard]] auto base() const noexcept
    {
        return static_cast<const T*>(m_light);
    }

    [[nodiscard]] auto& renderer() const noexcept { return m_renderer; }
    [[nodiscard]] virtual luisa::unique_ptr<Closure> closure(Expr<float> time) const noexcept = 0;
};

class Light::Closure
{
private:
    const Instance* m_instance;

private:
    Float m_time;

public:
    explicit Closure(const Instance* instance, Expr<float> time) noexcept
        : m_instance{instance}, m_time{time} {}
    virtual ~Closure() noexcept = default;

    Closure()                          = delete;
    Closure(const Closure&)            = delete;
    Closure& operator=(const Closure&) = delete;
    Closure(Closure&&)                 = delete;
    Closure& operator=(Closure&&)      = delete;

public:
    template <typename T = Instance>
        requires std::is_base_of_v<Instance, T>
    [[nodiscard]] auto instance() const noexcept
    {
        return static_cast<const T*>(m_instance);
    }

    [[nodiscard]] auto time() const noexcept { return m_time; }

    [[nodiscard]] virtual Evaluation evaluate(const Interaction& it_light, Expr<float3> p_from) const noexcept = 0;
};

} // namespace Yutrel

LUISA_STRUCT(Yutrel::Light::Handle, instance_id, light_tag){};