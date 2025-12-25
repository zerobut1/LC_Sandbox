#include "texture.h"

#include "textures/constant.h"

namespace Yutrel
{
    luisa::unique_ptr<Texture> Texture::create(const CreateInfo& info) noexcept
    {
        switch (info.type)
        {
        case Type::constant:
            return luisa::make_unique<ConstantTexture>(info);
        default:
            LUISA_ERROR("Unsupported texture");
            return nullptr;
        }
    }

} // namespace Yutrel