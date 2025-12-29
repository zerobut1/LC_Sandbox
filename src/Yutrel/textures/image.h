#pragma once

#include "base/texture.h"
#include "utils/image_io.h"

namespace Yutrel
{
    class ImageTexture final : public Texture
    {
    public:
        class Instance final : public Texture::Instance
        {
        private:
            uint m_texture_id;

        public:
            explicit Instance(const Renderer& renderer, const Texture* texture, uint texture_id) noexcept
                : Texture::Instance(renderer, texture), m_texture_id(texture_id) {}
            ~Instance() noexcept override = default;

            [[nodiscard]] Float4 evaluate(const RTWeekend::HitRecord& rec) const noexcept override;

            [[nodiscard]] Float4 decode(Expr<float4> rgba) const noexcept;
        };

    private:
        LoadedImage m_image;
        TextureSampler m_sampler;
        Encoding m_encoding;

    public:
        explicit ImageTexture(Scene& scene, const Texture::CreateInfo& info) noexcept;
        ~ImageTexture() noexcept override = default;

    public:
        [[nodiscard]] luisa::unique_ptr<Texture::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;

        [[nodiscard]] auto encoding() const noexcept { return m_encoding; }
    };

} // namespace Yutrel