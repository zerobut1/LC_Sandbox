#include "utils.h"

#include <luisa/luisa-compute.h>

namespace Utils
{
    Callable<float(float2, float)> plot_callable()
    {
        Callable plot = [](Float2 uv, Float y) noexcept -> Float
        {
            uv.y = 1.0f - uv.y;
            return saturate(smoothstep(y - 0.01f, y, uv.y) - smoothstep(y, y + 0.01f, uv.y));
        };
        return plot;
    }
} // namespace Utils