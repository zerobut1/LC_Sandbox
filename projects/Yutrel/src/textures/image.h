#pragma once

#include <utility>

#include "base/texture.h"
#include "scene/spec_base.h"
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

        [[nodiscard]] Float4 evaluate(const Interaction& it, Expr<float> time) const noexcept override;

        [[nodiscard]] Float4 decode(Expr<float4> rgba) const noexcept;
    };

private:
    LoadedImage m_image;
    TextureSampler m_sampler;
    Encoding m_encoding;

public:
    ImageTexture(luisa::filesystem::path path, TextureSampler sampler, Encoding encoding) noexcept;
    ~ImageTexture() noexcept override = default;

public:
    [[nodiscard]] luisa::unique_ptr<Texture::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;

    [[nodiscard]] auto encoding() const noexcept { return m_encoding; }
};

class ImageTextureSpec final : public TextureSpec
{
private:
    luisa::filesystem::path _path;
    TextureSampler _sampler;
    Texture::Encoding _encoding;

public:
    ImageTextureSpec(luisa::filesystem::path path, TextureSampler sampler, Texture::Encoding encoding) noexcept
        : _path{std::move(path)}, _sampler{sampler}, _encoding{encoding} {}

    [[nodiscard]] const Texture* build(SceneBuilder& builder) const noexcept override;
};

} // namespace Yutrel
