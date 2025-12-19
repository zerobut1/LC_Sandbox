#pragma once

#include <luisa/dsl/syntax.h>

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    [[nodiscard]] Float2 sample_uniform_disk_concentric(Expr<float2> u) noexcept;
    [[nodiscard]] Float3 sample_cosine_hemisphere(Expr<float2> u) noexcept;
    [[nodiscard]] Float3 sample_uniform_sphere(Expr<float2> u) noexcept;
    
} // namespace Yutrel