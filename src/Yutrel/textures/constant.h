#pragma once

#include "base/texture.h"

namespace Yutrel
{
    class ConstantTexture final : public Yutrel::Texture
    {
    public:
        class Instance final : public Yutrel::Texture::Instance
        {
        public:
            explicit Instance(const Yutrel::Renderer& renderer, const Yutrel::Texture* texture) noexcept
                : Yutrel::Texture::Instance(renderer, texture) {}
            ~Instance() noexcept override = default;

            Float4 evaluate() const noexcept override;
        };

    private:
        float4 v;

    public:
        explicit ConstantTexture(const Texture::CreateInfo& info) noexcept;
        ~ConstantTexture() noexcept override = default;

        ConstantTexture(const ConstantTexture&)            = delete;
        ConstantTexture& operator=(const ConstantTexture&) = delete;
        ConstantTexture(ConstantTexture&&)                 = delete;
        ConstantTexture& operator=(ConstantTexture&&)      = delete;

    public:
        [[nodiscard]] luisa::unique_ptr<Yutrel::Texture::Instance> build(const Yutrel::Renderer& renderer, const Yutrel::CommandBuffer& command_buffer) const noexcept override;
    };
} // namespace Yutrel