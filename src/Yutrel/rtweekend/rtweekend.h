#pragma once

#include <luisa/dsl/syntax.h>

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    [[nodiscard]] Float hit_sphere(Float3 center, Float radius, Var<Ray> ray) noexcept;

} // namespace Yutrel