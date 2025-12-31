#pragma once

#include "base/scene.h"
#include "base/texture.h"

namespace Yutrel
{
class ConstantTexture final : public Texture
{
public:
    class Instance final : public Texture::Instance
    {
    public:
        explicit Instance(const Renderer& renderer, const Texture* texture) noexcept
            : Texture::Instance(renderer, texture) {}
        ~Instance() noexcept override = default;

        Float4 evaluate(const Interaction& it) const noexcept override;
    };

private:
    float4 m_v;

public:
    explicit ConstantTexture(Scene& scene, const Texture::CreateInfo& info) noexcept;
    ~ConstantTexture() noexcept override = default;

public:
    [[nodiscard]] luisa::unique_ptr<Texture::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
};
} // namespace Yutrel