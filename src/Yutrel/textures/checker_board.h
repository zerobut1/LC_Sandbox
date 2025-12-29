#pragma once

#include "base/texture.h"

namespace Yutrel
{
    class CheckerBoard : public Texture
    {
    public:
        class Instance final : public Texture::Instance
        {
        private:
            const Texture::Instance* m_even;
            const Texture::Instance* m_odd;

        public:
            explicit Instance(const Renderer& renderer, const Texture* texture,
                              const Texture::Instance* even, const Texture::Instance* odd) noexcept
                : Texture::Instance(renderer, texture), m_even(even), m_odd(odd) {}
            ~Instance() noexcept override = default;

            Float4 evaluate(const RTWeekend::HitRecord& rec) const noexcept override;
        };

    private:
        float m_scale;
        const Texture* m_even;
        const Texture* m_odd;

    public:
        explicit CheckerBoard(Scene& scene, const Texture::CreateInfo& info) noexcept;
        ~CheckerBoard() noexcept override = default;

    public:
        [[nodiscard]] luisa::unique_ptr<Texture::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
        [[nodiscard]] auto scale() const noexcept { return m_scale; }
    };

} // namespace Yutrel