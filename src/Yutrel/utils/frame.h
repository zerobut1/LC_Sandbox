#pragma once

#include <luisa/dsl/syntax.h>

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Frame
{
private:
    Float3 m_s;
    Float3 m_t;
    Float3 m_n;

public:
    Frame() noexcept;
    Frame(Expr<float3> s, Expr<float3> t, Expr<float3> n) noexcept;
    void flip() noexcept;
    [[nodiscard]] static Frame make(Expr<float3> n) noexcept;
    [[nodiscard]] static Frame make(Expr<float3> n, Expr<float3> s) noexcept;
    [[nodiscard]] Float3 local_to_world(Expr<float3> d) const noexcept;
    [[nodiscard]] Float3 world_to_local(Expr<float3> d) const noexcept;
    [[nodiscard]] Expr<float3> s() const noexcept { return m_s; }
    [[nodiscard]] Expr<float3> t() const noexcept { return m_t; }
    [[nodiscard]] Expr<float3> n() const noexcept { return m_n; }
};

[[nodiscard]] inline auto sqr(auto x) noexcept { return x * x; }
[[nodiscard]] inline auto one_minus_sqr(auto x) noexcept { return 1.f - sqr(x); }
[[nodiscard]] inline auto cos_theta(Expr<float3> w) { return w.z; }
[[nodiscard]] inline auto cos2_theta(Expr<float3> w) { return sqr(w.z); }
[[nodiscard]] inline auto abs_cos_theta(Expr<float3> w) { return abs(w.z); }
} // namespace Yutrel