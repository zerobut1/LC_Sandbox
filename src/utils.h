#pragma once

#include <luisa/dsl/sugar.h>

namespace Utils
{
    using namespace luisa;
    using namespace luisa::compute;

    Callable<float(float2, float)> plot_callable();

} // namespace Utils