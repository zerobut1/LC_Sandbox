#include "constant.h"

namespace Yutrel
{
    ConstantTexture::ConstantTexture(Scene& scene, const Texture::CreateInfo& info) noexcept
        : Texture(scene, info), m_v(info.v) {}

    luisa::unique_ptr<Texture::Instance> ConstantTexture::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
    {
        return luisa::make_unique<ConstantTexture::Instance>(renderer, this);
    }

    Float4 ConstantTexture::Instance::evaluate(const RTWeekend::HitRecord& rec) const noexcept
    {
        return base<ConstantTexture>()->m_v;
    }
} // namespace Yutrel