#pragma once

#include "base/filter.h"

namespace Yutrel
{
class GaussianFilter : public Filter
{
private:
    float m_sigma;

public:
    explicit GaussianFilter(const Scene& scene, const CreateInfo& info) noexcept
        : Filter(scene, info)
    {
        m_sigma = radius() / 3.0f;
    }

    [[nodiscard]] float evaluate(float x) const noexcept override
    {
        auto G = [s = 2.0f * m_sigma * m_sigma](auto x) noexcept
        {
            return 1.0f / std::sqrt(pi * s) * std::exp(-x * x / s);
        };
        return G(x) - G(radius());
    }
};
} // namespace Yutrel