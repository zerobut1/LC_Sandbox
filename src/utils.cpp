#include "utils.h"

#include <luisa/luisa-compute.h>

namespace Utils
{
    Float plot(Float2 uv, Float y) noexcept
    {
        static Callable impl = [](Float2 uv, Float y) noexcept -> Float
        {
            uv.y = 1.0f - uv.y;
            return saturate(smoothstep(y - 0.01f, y, uv.y) - smoothstep(y, y + 0.01f, uv.y));
        };
        return impl(uv, y);
    }

    Float3 rgb2hsb(Float3 c) noexcept
    {
        static Callable impl = [](Float3 c) noexcept -> Float3
        {
            Float4 K   = make_float4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
            Float4 p   = lerp(make_float4(c.z, c.y, K.w, K.z),
                            make_float4(c.y, c.z, K.x, K.y),
                            step(c.z, c.y));
            Float4 q   = lerp(make_float4(p.x, p.y, p.w, c.x),
                            make_float4(c.x, p.y, p.z, p.x),
                            step(p.x, c.x));
            Float d    = q.x - min(q.w, q.y);
            Float e    = 1.0e-10f;
            Float3 hsb = make_float3(abs(q.z + (q.w - q.y) / (6.0f * d + e)),
                                     d / (q.x + e),
                                     q.x);
            return hsb;
        };
        return impl(c);
    }

    Float3 hsb2rgb(Float3 c) noexcept
    {
        static Callable impl = [](Float3 c) noexcept -> Float3
        {
            Float3 rgb = saturate(abs(mod(c.x * 6.0f + make_float3(0.0f, 4.0f, 2.0f), 6.0f) - 3.0f) - 1.0f);
            rgb        = rgb * rgb * (3.0f - 2.0f * rgb);
            rgb        = c.z * lerp(make_float3(1.0f), rgb, c.y);

            return rgb;
        };

        return impl(c);
    }

    Float box(Float2 st, Float2 size) noexcept
    {
        static Callable impl = [](Float2 st, Float2 size) noexcept -> Float
        {
            size = 0.5f - size * 0.5f;

            Float2 uv = smoothstep(size, size + 0.001f, st);
            uv *= smoothstep(size, size + 0.001f, 1.0f - st);
            return uv.x * uv.y;
        };
        return impl(st, size);
    }

    Float circle(Float2 st, Float radius) noexcept
    {
        static Callable impl = [](Float2 st, Float radius) noexcept -> Float
        {
            Float2 l = st - 0.5f;

            return smoothstep(radius + radius * 0.01f, radius - radius * 0.01f, dot(l, l) * 4.0f);
        };
        return impl(st, radius);
    }

    Float2x2 rotate2d(Float angle) noexcept
    {
        static Callable impl = [](Float angle) noexcept -> Float2x2
        {
            Float s = sin(angle);
            Float c = cos(angle);

            return make_float2x2(c, -s, s, c);
        };
        return impl(angle);
    }

    Float2x2 scale2d(Float2 scale) noexcept
    {
        static Callable impl = [](Float2 scale) noexcept -> Float2x2
        {
            return make_float2x2(scale.x, 0.0f, 0.0f, scale.y);
        };
        return impl(scale);
    }

    Float2 tile(Float2 st, Float zoom) noexcept
    {
        static Callable impl = [](Float2 st, Float zoom) noexcept -> Float2
        {
            st *= zoom;
            return fract(st);
        };
        return impl(st, zoom);
    }

    Float2 rotate2d(Float2 st, Float angle) noexcept
    {
        static Callable impl = [](Float2 st, Float angle) noexcept -> Float2
        {
            st -= 0.5f;
            st = rotate2d(angle) * st;
            st += 0.5f;

            return st;
        };
        return impl(st, angle);
    }

    UInt pcg(UInt v) noexcept
    {
        static Callable impl = [](UInt v) noexcept -> UInt
        {
            UInt state = v * 747796405u + 2891336453u;
            UInt word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
            return (word >> 22u) ^ word;
        };
        return impl(v);
    }

    Float random(Float p) noexcept
    {
        static Callable impl = [](Float p) noexcept -> Float
        {
            return cast<Float>(pcg(cast<UInt>(p))) / cast<Float>(0xFFFFFFFFu);
        };
        return impl(p);
    }

    Float random(Float2 p) noexcept
    {
        static Callable impl = [](Float2 p) noexcept -> Float
        {
            return cast<Float>(pcg(pcg(cast<UInt>(p.x)) + cast<UInt>(p.y))) / cast<Float>(0xFFFFFFFFu);
        };
        return impl(p);
    }

    Float noise(Float2 st) noexcept
    {
        static Callable impl = [](Float2 st) noexcept -> Float
        {
            Float2 i = floor(st);
            Float2 f = fract(st);

            Float a = random(i);
            Float b = random(i + make_float2(1.0f, 0.0f));
            Float c = random(i + make_float2(0.0f, 1.0f));
            Float d = random(i + make_float2(1.0f, 1.0f));

            Float2 u = smoothstep(0.0f, 1.0f, f);

            return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
        };
        return impl(st);
    }

} // namespace Utils