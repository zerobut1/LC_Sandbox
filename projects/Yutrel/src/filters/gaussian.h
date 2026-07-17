#pragma once

#include "base/filter.h"
#include "scene/scene_builder.h"

namespace Yutrel
{
class GaussianFilter : public Filter
{
private:
    float m_sigma;

public:
    explicit GaussianFilter(float radius) noexcept
        : Filter{radius}
    {
        m_sigma = this->radius() / 3.0f;
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

class GaussianFilterSpec final : public FilterSpec
{
private:
    float _radius;

public:
    explicit GaussianFilterSpec(float radius) noexcept : _radius{radius} {}
    [[nodiscard]] luisa::optional<luisa::string> validate() const noexcept override { return std::isfinite(_radius) && _radius > 0.0f ? luisa::nullopt : spec_validation_error("Filter radius must be finite and positive."); }
    [[nodiscard]] const Filter* build(SceneBuilder& builder) const noexcept override
    {
        return builder.emplace<Filter, GaussianFilter>(_radius);
    }
};
} // namespace Yutrel
