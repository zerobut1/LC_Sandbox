#include "checker_board.h"

#include "base/interaction.h"
#include "base/renderer.h"
#include "scene/scene_builder.h"

namespace Yutrel
{
CheckerBoard::CheckerBoard(float scale, const Texture* even, const Texture* odd) noexcept
    : m_scale{scale},
      m_even{even},
      m_odd{odd} {}

luisa::unique_ptr<Texture::Instance> CheckerBoard::build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept
{
    auto even = renderer.build_texture(command_buffer, m_even);
    auto odd  = renderer.build_texture(command_buffer, m_odd);
    return luisa::make_unique<CheckerBoard::Instance>(renderer, this, even, odd);
}

Float4 CheckerBoard::Instance::evaluate(const Interaction& it, Expr<float> time) const noexcept
{
    auto inv_scale = 1.0f / base<CheckerBoard>()->scale();
    auto position  = it.p_g;

    auto x_integer = static_cast<Int>(floor(inv_scale * position.x));
    auto y_integer = static_cast<Int>(floor(inv_scale * position.y));
    auto z_integer = static_cast<Int>(floor(inv_scale * position.z));

    auto is_even = (x_integer + y_integer + z_integer) % 2 == 0;
    return ite(is_even, m_even->evaluate(it, time), m_odd->evaluate(it, time));
}

const Texture* CheckerBoardTextureSpec::build(SceneBuilder& builder) const noexcept
{
    return builder.emplace<Texture, CheckerBoard>(_scale, builder.resolve(_even), builder.resolve(_odd));
}
} // namespace Yutrel
