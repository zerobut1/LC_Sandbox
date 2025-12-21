#pragma once

#include <luisa/dsl/sugar.h>
#include <luisa/dsl/syntax.h>

namespace Yutrel::RTWeekend
{
    using namespace luisa;
    using namespace luisa::compute;

    class AABB
    {
    private:
        float3 m_min;
        float3 m_max;

    public:
        AABB() noexcept = default;
        AABB(float3 a, float3 b) noexcept
        {
            m_min = luisa::min(a, b);
            m_max = luisa::max(a, b);
        }
        AABB(const AABB& box0, const AABB& box1) noexcept
        {
            m_min = luisa::min(box0.m_min, box1.m_min);
            m_max = luisa::max(box0.m_max, box1.m_max);
        }

        [[nodiscard]] float3 min() const noexcept
        {
            return m_min;
        }

        [[nodiscard]] float3 max() const noexcept
        {
            return m_max;
        }

        [[nodiscard]] Bool hit(Var<Ray> r, Float t_min, Float t_max) const noexcept
        {
            Bool ret = true;

            for (auto axis = 0u; axis < 3u; axis++)
            {
                $if(ret)
                {
                    auto inv_d = 1.0f / r->direction()[axis];
                    auto t0    = (m_min[axis] - r->origin()[axis]) * inv_d;
                    auto t1    = (m_max[axis] - r->origin()[axis]) * inv_d;

                    $if(inv_d < 0.0f)
                    {
                        auto tmp = t0;
                        t0       = t1;
                        t1       = tmp;
                    };
                    t_min = compute::max(t0, t_min);
                    t_max = compute::min(t1, t_max);

                    $if(t_max <= t_min)
                    {
                        ret = false;
                    };
                };
            }
            return ret;
        }
    };
} // namespace Yutrel::RTWeekend