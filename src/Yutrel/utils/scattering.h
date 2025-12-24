#pragma once

#include <luisa/dsl/syntax.h>

#include "frame.h"

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    [[nodiscard]] Bool refract(Expr<float3> wi, Expr<float3> n, Expr<float> eta, Float3* wt) noexcept;

} // namespace Yutrel