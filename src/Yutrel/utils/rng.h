#pragma once

#include <luisa/dsl/syntax.h>

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    [[nodiscard]] UInt xxhash32(Expr<uint> p) noexcept;
    [[nodiscard]] UInt xxhash32(Expr<uint2> p) noexcept;
    [[nodiscard]] UInt xxhash32(Expr<uint3> p) noexcept;
    [[nodiscard]] UInt xxhash32(Expr<uint4> p) noexcept;

    [[nodiscard]] Float uniform_uint_to_float(Expr<uint> u) noexcept;
    
    [[nodiscard]] Float lcg(UInt& state) noexcept;

} // namespace Yutrel