#pragma once

#include <luisa/dsl/sugar.h>

namespace Utils
{
    using namespace luisa;
    using namespace luisa::compute;

    [[nodiscard]] Float plot(Float2 uv, Float y) noexcept;

    [[nodiscard]] Float3 rgb2hsb(Float3 c) noexcept;
    [[nodiscard]] Float3 hsb2rgb(Float3 c) noexcept;

} // namespace Utils