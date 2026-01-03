#pragma once

#include <luisa/dsl/syntax.h>

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

[[nodiscard]] Float2 sample_uniform_disk_concentric(Expr<float2> u) noexcept;
[[nodiscard]] Float3 sample_cosine_hemisphere(Expr<float2> u) noexcept;
[[nodiscard]] Float3 sample_uniform_sphere(Expr<float2> u) noexcept;

struct AliasEntry
{
    float prob;
    uint alias;
};
// reference: https://github.com/AirGuanZ/agz-utils
[[nodiscard]] std::pair<luisa::vector<AliasEntry>, luisa::vector<float> /* pdf */>
create_alias_table(luisa::span<const float> values) noexcept;

[[nodiscard]] Float balance_heuristic(Expr<uint> nf, Expr<float> fPdf, Expr<uint> ng, Expr<float> gPdf) noexcept;
[[nodiscard]] Float power_heuristic(Expr<uint> nf, Expr<float> fPdf, Expr<uint> ng, Expr<float> gPdf) noexcept;
[[nodiscard]] Float balance_heuristic(Expr<float> fPdf, Expr<float> gPdf) noexcept;
[[nodiscard]] Float power_heuristic(Expr<float> fPdf, Expr<float> gPdf) noexcept;

} // namespace Yutrel