#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>

namespace Yutrel::RTWeekend
{
    struct HitRecord;
}

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Renderer;
    class CommandBuffer;
    class Scene;

    class Texture
    {
    public:
        enum class Type
        {
            constant,
            checker_board,
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
            [[nodiscard]] virtual Float4 evaluate(const RTWeekend::HitRecord& rec) const noexcept = 0;
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
    };
} // namespace Yutrel