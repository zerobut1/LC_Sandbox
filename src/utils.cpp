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
            rgb = rgb * rgb * (3.0f - 2.0f * rgb);
            rgb = c.z * lerp(make_float3(1.0f), rgb, c.y);

            return rgb;
        };

        return impl(c);
    }

} // namespace Utils