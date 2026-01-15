#pragma once

#include "base/filter.h"

namespace Yutrel
{
class MitchellFilter : public Filter
{
private:
    float m_b{1.0f / 3.0f};
    float m_c{1.0f / 3.0f};

public:
    explicit MitchellFilter(const Scene& scene, const CreateInfo& info) noexcept
        : Filter(scene, info) {}

    [[nodiscard]] float evaluate(float x) const noexcept override
    {
        x = 2.f * std::abs(x / radius());
        if (x <= 1.0f)
        {
            return ((12.0f - 9.0f * m_b - 6.0f * m_c) * x * x * x +
                    (-18.0f + 12.0f * m_b + 6.0f * m_c) * x * x +
                    (6.0f - 2.0f * m_b)) *
                   (1.f / 6.f);
        }
        if (x <= 2.0f)
        {
            return ((-m_b - 6.0f * m_c) * x * x * x +
                    (6.0f * m_b + 30.0f * m_c) * x * x +
                    (-12.0f * m_b - 48.0f * m_c) * x +
                    (8.0f * m_b + 24.0f * m_c)) *
                   (1.0f / 6.0f);
        }
        return 0.0f;
    }
};
} // namespace Yutrel