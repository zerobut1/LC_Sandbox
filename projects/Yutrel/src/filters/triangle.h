#pragma once

#include <algorithm>
#include <cmath>

#include "base/filter.h"

namespace Yutrel
{
class TriangleFilter final : public Filter
{
public:
    explicit TriangleFilter(const Scene& scene, const CreateInfo& info) noexcept
        : Filter(scene, info) {}

    [[nodiscard]] float evaluate(float x) const noexcept override
    {
        return std::max(0.0f, radius() - std::abs(x));
    }
};
} // namespace Yutrel
