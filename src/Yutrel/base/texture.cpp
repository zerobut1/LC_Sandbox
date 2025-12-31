#include "texture.h"

#include "textures/checker_board.h"
#include "textures/constant.h"
#include "textures/image.h"

namespace Yutrel
{
luisa::unique_ptr<Texture> Texture::create(Scene& scene, const CreateInfo& info) noexcept
{
    switch (info.type)
    {
    case Type::constant:
        return luisa::make_unique<ConstantTexture>(scene, info);
    case Type::checker_board:
        return luisa::make_unique<CheckerBoard>(scene, info);
    case Type::image:
        return luisa::make_unique<ImageTexture>(scene, info);
    default:
        LUISA_ERROR("Unsupported texture");
        return nullptr;
    }
}

} // namespace Yutrel