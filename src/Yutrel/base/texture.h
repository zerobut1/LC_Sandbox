#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;
using TextureSampler = compute::Sampler;

class Scene;
class Renderer;
class CommandBuffer;
class Interaction;

class Texture
{
public:
    enum class Type
    {
        constant,
        checker_board,
        image,
    };

    enum class Encoding : uint
    {
        LINEAR,
        SRGB,
        GAMMA,
    };

    struct CreateInfo
    {
        Type type{Type::constant};
        // constant
        float4 v;
        // checker_board
        float scale{1.0f};
        float4 even;
        float4 odd;
        // image
        luisa::filesystem::path path;
        TextureSampler sampler;
        Encoding encoding{Encoding::LINEAR};
    };

    [[nodiscard]] static luisa::unique_ptr<Texture> create(Scene& scene, const CreateInfo& info) noexcept;

public:
    class Instance
    {
    private:
        const Renderer& m_renderer;
        const Texture* m_texture;

    public:
        explicit Instance(const Renderer& renderer, const Texture* texture) noexcept
            : m_renderer(renderer), m_texture(texture) {}
        virtual ~Instance() noexcept = default;

        template <typename T = Texture>
            requires std::is_base_of_v<Texture, T>
        [[nodiscard]] auto base() const noexcept
        {
            return static_cast<const T*>(m_texture);
        }
        Instance()                           = delete;
        Instance(const Instance&)            = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&)                 = delete;
        Instance& operator=(Instance&&)      = delete;

    public:
        [[nodiscard]] auto& renderer() const noexcept { return m_renderer; }

        [[nodiscard]] virtual Float4 evaluate(const Interaction& it) const noexcept = 0;
    };

public:
    explicit Texture(Scene& scene, const Texture::CreateInfo& info) noexcept {}
    virtual ~Texture() noexcept = default;

    Texture()                          = delete;
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&)                 = delete;
    Texture& operator=(Texture&&)      = delete;

public:
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept = 0;

    // TODO 
    // is black
    // is constant
};
} // namespace Yutrel