#include "checker_board.h"

#include "base/interaction.h"
#include "base/renderer.h"
#include "base/scene.h"

namespace Yutrel
{
CheckerBoard::CheckerBoard(Scene& scene, const Texture::CreateInfo& info) noexcept
    : Texture(scene, info),
      m_scale(info.scale)
{
    Texture::CreateInfo even_info{
        .type = Type::constant,
        .v    = info.even,
    };
    m_even = scene.load_texture(even_info);
    Texture::CreateInfo odd_info{
        .type = Type::constant,
        .v    = info.odd,
    };
    m_odd = scene.load_texture(odd_info);
}

luisa::unique_ptr<Texture::Instance> CheckerBoard::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto even = renderer.build_texture(command_buffer, m_even);
    auto odd  = renderer.build_texture(command_buffer, m_odd);
    return luisa::make_unique<CheckerBoard::Instance>(renderer, this, even, odd);
}

Float4 CheckerBoard::Instance::evaluate(const Interaction& it) const noexcept
{
    auto inv_scale = 1.0f / base<CheckerBoard>()->scale();
    auto position  = it.p_g;

    auto x_integer = static_cast<Int>(floor(inv_scale * position.x));
    auto y_integer = static_cast<Int>(floor(inv_scale * position.y));
    auto z_integer = static_cast<Int>(floor(inv_scale * position.z));

    auto is_even = (x_integer + y_integer + z_integer) % 2 == 0;
    return ite(is_even, m_even->evaluate(it), m_odd->evaluate(it));
}
} // namespace Yutrel