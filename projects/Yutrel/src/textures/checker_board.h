#pragma once

#include <cmath>

#include "base/texture.h"
#include "scene/spec_base.h"

namespace Yutrel
{
class CheckerBoard : public Texture
{
public:
    class Instance final : public Texture::Instance
    {
    private:
        const Texture::Instance* m_even;
        const Texture::Instance* m_odd;

    public:
        explicit Instance(const Renderer& renderer, const Texture* texture,
                          const Texture::Instance* even, const Texture::Instance* odd) noexcept
            : Texture::Instance(renderer, texture), m_even(even), m_odd(odd) {}
        ~Instance() noexcept override = default;

        Float4 evaluate(const Interaction& it, Expr<float> time) const noexcept override;
    };

private:
    float m_scale;
    const Texture* m_even;
    const Texture* m_odd;

public:
    CheckerBoard(float scale, const Texture* even, const Texture* odd) noexcept;
    ~CheckerBoard() noexcept override = default;

public:
    [[nodiscard]] luisa::unique_ptr<Texture::Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override;
    [[nodiscard]] auto scale() const noexcept { return m_scale; }
};

class CheckerBoardTextureSpec final : public TextureSpec
{
private:
    float _scale;
    TextureRef _even;
    TextureRef _odd;

public:
    CheckerBoardTextureSpec(float scale, TextureRef even, TextureRef odd) noexcept
        : _scale{scale}, _even{even}, _odd{odd} {}

    [[nodiscard]] luisa::optional<luisa::string> validate() const noexcept override
    {
        return std::isfinite(_scale) && _scale > 0.0f
                   ? luisa::nullopt
                   : spec_validation_error("Checker-board texture scale must be finite and positive.");
    }
    void visit_dependencies(SpecDependencyVisitor& visitor) const noexcept override
    {
        visitor.visit(_even);
        visitor.visit(_odd);
    }

    [[nodiscard]] const Texture* build(SceneBuilder& builder) const noexcept override;
};

} // namespace Yutrel
