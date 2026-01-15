#pragma once

#include "base/filter.h"

namespace Yutrel
{
class LanczosSincFilter final : public Filter
{
private:
    float m_tau{3.0f};

public:
    explicit LanczosSincFilter(const Scene& scene, const CreateInfo& info) noexcept
        : Filter(scene, info) {}

    [[nodiscard]] float evaluate(float x) const noexcept override
    {
        x = x / radius();

        static constexpr auto sin_x_over_x = [](auto x) noexcept
        {
            return 1.0f + x * x == 1.0f ? 1.0f : std::sin(x) / x;
        };
        static constexpr auto sinc = [](auto x) noexcept
        {
            return sin_x_over_x(pi * x);
        };
        if (std::abs(x) > 1.0f) [[unlikely]]
        {
            return 0.0f;
        }
        return sinc(x) * sinc(x / m_tau);
    }
};

} // namespace Yutrel
