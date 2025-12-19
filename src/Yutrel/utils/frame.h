#pragma once

#include <luisa/dsl/syntax.h>

namespace Yutrel
{

    [[nodiscard]] inline auto sqr(auto x) noexcept { return x * x; }
    [[nodiscard]] inline auto one_minus_sqr(auto x) noexcept { return 1.f - sqr(x); }
} // namespace Yutrel