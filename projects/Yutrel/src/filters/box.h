#pragma once

#include "base/filter.h"

namespace Yutrel
{
class BoxFilter : public Filter
{
public:
    explicit BoxFilter(float radius) noexcept
        : Filter{radius} {}

    [[nodiscard]] float evaluate(float x) const noexcept override
    {
        return 1.0f;
    }
};

class BoxFilterSpec final : public FilterSpec
{
private:
    float _radius;

public:
    explicit BoxFilterSpec(float radius) noexcept : _radius{radius} {}
    [[nodiscard]] luisa::optional<luisa::string> validate() const noexcept override { return std::isfinite(_radius) && _radius > 0.0f ? luisa::nullopt : spec_validation_error("Filter radius must be finite and positive."); }
    [[nodiscard]] const Filter* build(SceneBuilder& builder) const noexcept override;
};
} // namespace Yutrel
