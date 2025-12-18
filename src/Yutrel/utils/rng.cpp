#include "rng.h"

#include <luisa/luisa-compute.h>

namespace Yutrel
{
    UInt xxhash32(Expr<uint> p) noexcept
    {
        static Callable impl = [](UInt p) noexcept
        {
            constexpr auto PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
            constexpr auto PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
            auto h32 = p + PRIME32_5;
            h32      = PRIME32_4 * ((h32 << 17u) | (h32 >> (32u - 17u)));
            h32      = PRIME32_2 * (h32 ^ (h32 >> 15u));
            h32      = PRIME32_3 * (h32 ^ (h32 >> 13u));
            return h32 ^ (h32 >> 16u);
        };
        return impl(p);
    }

    UInt xxhash32(Expr<uint2> p) noexcept
    {
        static Callable impl = [](UInt2 p) noexcept
        {
            constexpr auto PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
            constexpr auto PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
            auto h32 = p.y + PRIME32_5 + p.x * PRIME32_3;
            h32      = PRIME32_4 * ((h32 << 17u) | (h32 >> (32u - 17u)));
            h32      = PRIME32_2 * (h32 ^ (h32 >> 15u));
            h32      = PRIME32_3 * (h32 ^ (h32 >> 13u));
            return h32 ^ (h32 >> 16u);
        };
        return impl(p);
    }

    UInt xxhash32(Expr<uint3> p) noexcept
    {
        static Callable impl = [](UInt3 p) noexcept
        {
            constexpr auto PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
            constexpr auto PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
            UInt h32 = p.z + PRIME32_5 + p.x * PRIME32_3;
            h32      = PRIME32_4 * ((h32 << 17u) | (h32 >> (32u - 17u)));
            h32 += p.y * PRIME32_3;
            h32 = PRIME32_4 * ((h32 << 17u) | (h32 >> (32u - 17u)));
            h32 = PRIME32_2 * (h32 ^ (h32 >> 15u));
            h32 = PRIME32_3 * (h32 ^ (h32 >> 13u));
            return h32 ^ (h32 >> 16u);
        };
        return impl(p);
    }

    UInt xxhash32(Expr<uint4> p) noexcept
    {
        static Callable impl = [](UInt4 p) noexcept
        {
            constexpr auto PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
            constexpr auto PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
            auto h32 = p.w + PRIME32_5 + p.x * PRIME32_3;
            h32      = PRIME32_4 * ((h32 << 17u) | (h32 >> (32u - 17u)));
            h32 += p.y * PRIME32_3;
            h32 = PRIME32_4 * ((h32 << 17u) | (h32 >> (32u - 17u)));
            h32 += p.z * PRIME32_3;
            h32 = PRIME32_4 * ((h32 << 17u) | (h32 >> (32u - 17u)));
            h32 = PRIME32_2 * (h32 ^ (h32 >> 15u));
            h32 = PRIME32_3 * (h32 ^ (h32 >> 13u));
            return h32 ^ (h32 >> 16u);
        };
        return impl(p);
    }

    Float uniform_uint_to_float(Expr<uint> u) noexcept
    {
        return min(one_minus_epsilon, u * 0x1p-32f);
    }

    Float lcg(UInt& state) noexcept
    {
        static Callable impl = [](UInt& state) noexcept
        {
            constexpr auto lcg_a = 1664525u;
            constexpr auto lcg_c = 1013904223u;
            state                = lcg_a * state + lcg_c;
            return uniform_uint_to_float(state);
        };
        return impl(state);
    }
} // namespace Yutrel