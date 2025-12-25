#include "constant.h"

namespace Yutrel
{
    ConstantTexture::ConstantTexture(const Texture::CreateInfo& info) noexcept
        : Texture(info), v(info.v) {}

    luisa::unique_ptr<Texture::Instance> ConstantTexture::build(const Renderer& renderer, const CommandBuffer& command_buffer) const noexcept
    {
        return luisa::make_unique<ConstantTexture::Instance>(renderer, this);
    }

    Float4 ConstantTexture::Instance::evaluate() const noexcept
    {
        return base<ConstantTexture>()->v;
    }
} // namespace Yutrel