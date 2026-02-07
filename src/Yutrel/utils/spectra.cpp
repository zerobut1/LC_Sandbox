#include "spectra.h"

namespace Yutrel
{
SampledSpectrum zero_if_any_nan(const SampledSpectrum& t) noexcept
{
    auto any_nan = t.any([](const auto& value)
    {
        return luisa::compute::isnan(value);
    });
    return t.map([&any_nan](auto x) noexcept
    {
        return ite(any_nan, 0.f, x);
    });
}

SampledSpectrum ite(const SampledSpectrum& p, const SampledSpectrum& t, const SampledSpectrum& f) noexcept
{
    auto n = std::max({p.dimension(), t.dimension(), f.dimension()});
    LUISA_ASSERT((p.dimension() == 1u || p.dimension() == n) &&
                     (t.dimension() == 1u || t.dimension() == n) &&
                     (f.dimension() == 1u || f.dimension() == n),
                 "Invalid spectrum dimensions for ite: (p = {}, t = {}, f = {}).",
                 p.dimension(),
                 t.dimension(),
                 f.dimension());
    auto r = SampledSpectrum{n};
    compute::outline([&]
    {
        for (auto i = 0u; i < n; i++)
        {
            r[i] = ite(p[i] != 0.f, t[i], f[i]);
        }
    });
    return r;
}

SampledSpectrum ite(const SampledSpectrum& p, Expr<float> t, const SampledSpectrum& f) noexcept
{
    LUISA_ASSERT(f.dimension() == 1u || p.dimension() == 1u || f.dimension() == p.dimension(),
                 "Invalid spectrum dimensions for ite: (p = {}, t = 1, f = {}).",
                 p.dimension(),
                 f.dimension());
    auto r = SampledSpectrum{std::max(f.dimension(), p.dimension())};
    compute::outline([&]
    {
        for (auto i = 0u; i < r.dimension(); i++)
        {
            r[i] = ite(p[i] != 0.f, t, f[i]);
        }
    });
    return r;
}

SampledSpectrum ite(const SampledSpectrum& p, const SampledSpectrum& t, Expr<float> f) noexcept
{
    LUISA_ASSERT(t.dimension() == 1u || p.dimension() == 1u || t.dimension() == p.dimension(),
                 "Invalid spectrum dimensions for ite: (p = {}, t = {}, f = 1).",
                 p.dimension(),
                 t.dimension());
    auto r = SampledSpectrum{std::max(t.dimension(), p.dimension())};
    compute::outline([&]
    {
        for (auto i = 0u; i < r.dimension(); i++)
        {
            r[i] = ite(p[i] != 0.f, t[i], f);
        }
    });
    return r;
}

SampledSpectrum ite(const SampledSpectrum& p, Expr<float> t, Expr<float> f) noexcept
{
    return p.map([t, f](auto x) noexcept
    {
        return ite(x != 0.f, t, f);
    });
}

SampledSpectrum ite(Expr<bool> p, const SampledSpectrum& t, const SampledSpectrum& f) noexcept
{
    LUISA_ASSERT(t.dimension() == 1u || f.dimension() == 1u || t.dimension() == f.dimension(),
                 "Invalid spectrum dimensions for ite: (p = 1, t = {}, f = {}).",
                 t.dimension(),
                 f.dimension());
    auto r = SampledSpectrum{std::max(t.dimension(), f.dimension())};
    compute::outline([&]
    {
        for (auto i = 0u; i < r.dimension(); i++)
        {
            r[i] = ite(p, t[i], f[i]);
        }
    });
    return r;
}

SampledSpectrum ite(Expr<bool> p, Expr<float> t, const SampledSpectrum& f) noexcept
{
    return f.map([p, t](auto i, auto x) noexcept
    {
        return ite(p, t, x);
    });
}

SampledSpectrum ite(Expr<bool> p, const SampledSpectrum& t, Expr<float> f) noexcept
{
    return t.map([p, f](auto i, auto x) noexcept
    {
        return ite(p, x, f);
    });
}

SampledSpectrum saturate(const SampledSpectrum& t) noexcept
{
    return t.map([](auto x) noexcept
    {
        return saturate(x);
    });
}

SampledSpectrum abs(const SampledSpectrum& t) noexcept
{
    return t.map([](auto x) noexcept
    {
        return abs(x);
    });
}

SampledSpectrum sqrt(const SampledSpectrum& t) noexcept
{
    return t.map([](auto x) noexcept
    {
        return sqrt(x);
    });
}

SampledSpectrum exp(const SampledSpectrum& t) noexcept
{
    return t.map([](auto x) noexcept
    {
        return exp(x);
    });
}

SampledSpectrum max(const SampledSpectrum& a, Expr<float> b) noexcept
{
    return a.map([b](auto x) noexcept
    {
        return max(x, b);
    });
}

SampledSpectrum max(Expr<float> a, const SampledSpectrum& b) noexcept
{
    return b.map([a](auto x) noexcept
    {
        return max(a, x);
    });
}

SampledSpectrum max(const SampledSpectrum& a, const SampledSpectrum& b) noexcept
{
    auto n = std::max({a.dimension(), b.dimension()});
    LUISA_ASSERT((a.dimension() == 1u || a.dimension() == n) &&
                     (b.dimension() == 1u || b.dimension() == n),
                 "Invalid spectrum dimensions for max: (a = {}, b = {}).",
                 a.dimension(),
                 b.dimension());
    auto ans = SampledSpectrum{n};
    compute::outline([&]
    {
        for (auto i = 0u; i < n; i++)
        {
            ans[i] = max(a[i], b[i]);
        }
    });
    return ans;
}

SampledSpectrum min(const SampledSpectrum& a, Expr<float> b) noexcept
{
    return a.map([b](auto x) noexcept
    {
        return min(x, b);
    });
}

SampledSpectrum min(Expr<float> a, const SampledSpectrum& b) noexcept
{
    return b.map([a](auto x) noexcept
    {
        return min(a, x);
    });
}

SampledSpectrum min(const SampledSpectrum& a, const SampledSpectrum& b) noexcept
{
    auto n = std::max({a.dimension(), b.dimension()});
    LUISA_ASSERT((a.dimension() == 1u || a.dimension() == n) &&
                     (b.dimension() == 1u || b.dimension() == n),
                 "Invalid spectrum dimensions for min: (a = {}, b = {}).",
                 a.dimension(),
                 b.dimension());
    auto ans = SampledSpectrum{n};
    compute::outline([&]
    {
        for (auto i = 0u; i < n; i++)
        {
            ans[i] = min(a[i], b[i]);
        }
    });
    return ans;
}

SampledSpectrum clamp(const SampledSpectrum& v, Expr<float> l, Expr<float> r) noexcept
{
    return v.map([l, r](auto x) noexcept
    {
        return clamp(x, l, r);
    });
}

SampledSpectrum clamp(const SampledSpectrum& v, const SampledSpectrum& l, Expr<float> r) noexcept
{
    auto n = std::max(v.dimension(), l.dimension());
    LUISA_ASSERT((v.dimension() == 1u || v.dimension() == n) &&
                     (l.dimension() == 1u || l.dimension() == n),
                 "Invalid spectrum dimensions for clamp: (v = {}, l = {}, r = 1).",
                 v.dimension(),
                 l.dimension());
    auto ans = SampledSpectrum{n};
    compute::outline([&]
    {
        for (auto i = 0u; i < n; i++)
        {
            ans[i] = clamp(v[i], l[i], r);
        }
    });
    return ans;
}

SampledSpectrum clamp(const SampledSpectrum& v, Expr<float> l, const SampledSpectrum& r) noexcept
{
    auto n = std::max(v.dimension(), r.dimension());
    LUISA_ASSERT((v.dimension() == 1u || v.dimension() == n) &&
                     (r.dimension() == 1u || r.dimension() == n),
                 "Invalid spectrum dimensions for clamp: (v = {}, l = 1, r = {}).",
                 v.dimension(),
                 r.dimension());
    auto ans = SampledSpectrum{n};
    compute::outline([&]
    {
        for (auto i = 0u; i < n; i++)
        {
            ans[i] = clamp(v[i], l, r[i]);
        }
    });
    return ans;
}

SampledSpectrum clamp(const SampledSpectrum& v, const SampledSpectrum& l, const SampledSpectrum& r) noexcept
{
    auto n = std::max({v.dimension(), l.dimension(), r.dimension()});
    LUISA_ASSERT((v.dimension() == 1u || v.dimension() == n) &&
                     (l.dimension() == 1u || l.dimension() == n) &&
                     (r.dimension() == 1u || r.dimension() == n),
                 "Invalid spectrum dimensions for clamp: (v = {}, l = {}, r = {}).",
                 v.dimension(),
                 l.dimension(),
                 r.dimension());
    auto ans = SampledSpectrum{n};
    compute::outline([&]
    {
        for (auto i = 0u; i < n; i++)
        {
            ans[i] = clamp(v[i], l[i], r[i]);
        }
    });
    return ans;
}

Bool any(const SampledSpectrum& v) noexcept
{
    return v.any([](auto x) noexcept
    {
        return x != 0.f;
    });
}

Bool all(const SampledSpectrum& v) noexcept
{
    return v.all([](auto x) noexcept
    {
        return x != 0.f;
    });
}

} // namespace Yutrel
