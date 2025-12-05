#pragma once

#include <luisa/dsl/sugar.h>

namespace Utils
{
    using namespace luisa;
    using namespace luisa::compute;

    [[nodiscard]] Float plot(Float2 uv, Float y) noexcept;

    [[nodiscard]] Float3 rgb2hsb(Float3 c) noexcept;
    [[nodiscard]] Float3 hsb2rgb(Float3 c) noexcept;

    [[nodiscard]] Float box(Float2 st, Float2 size) noexcept;
    [[nodiscard]] Float circle(Float2 st, Float radius) noexcept;

    [[nodiscard]] Float2x2 rotate2d(Float angle) noexcept;
    [[nodiscard]] Float2x2 scale2d(Float2 scale) noexcept;

} // namespace Utils