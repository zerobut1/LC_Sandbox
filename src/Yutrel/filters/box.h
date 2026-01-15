#pragma once

#include "base/filter.h"

namespace Yutrel
{
class BoxFilter : public Filter
{
public:
    explicit BoxFilter(const Scene& scene, const CreateInfo& info) noexcept
        : Filter(scene, info) {}

    [[nodiscard]] float evaluate(float x) const noexcept override
    {
        return 1.0f;
    }
};
} // namespace Yutrel