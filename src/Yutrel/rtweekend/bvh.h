#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>

#include "rtweekend/hittable_list.h"
#include "rtweekend/utils.h"

namespace Yutrel::RTWeekend
{
    class BVH_Node : public Hittable
    {
    private:
        luisa::shared_ptr<Hittable> m_left;
        luisa::shared_ptr<Hittable> m_right;
        AABB m_bbox;

    public:
        BVH_Node() noexcept = delete;

        BVH_Node(const luisa::vector<luisa::shared_ptr<Hittable>>& src_objects, size_t start, size_t end) noexcept
        {
            auto objects = src_objects;
            auto axis    = random_int(0, 2);

            auto comparator = axis == 0   ? box_x_compare
                              : axis == 1 ? box_y_compare
                                          : box_z_compare;

            auto object_span = end - start;

            if (object_span == 1)
            {
                m_left  = objects[start];
                m_right = objects[start];
            }
            else if (object_span == 2)
            {
                m_left  = objects[start];
                m_right = objects[start + 1];
            }
            else
            {
                std::ranges::sort(objects.begin() + start, objects.begin() + end, comparator);

                auto mid = start + object_span / 2;
                m_left   = luisa::make_shared<BVH_Node>(objects, start, mid);
                m_right  = luisa::make_shared<BVH_Node>(objects, mid, end);
            }

            m_bbox = AABB(m_left->bounding_box(), m_right->bounding_box());
        }

        BVH_Node(const HittableList& list) noexcept
            : BVH_Node(list.objects(), 0, list.objects().size())
        {
        }

        [[nodiscard]] Bool hit(Var<Ray> ray, Expr<float> t_min, Expr<float> t_max, Expr<float> time, HitRecord& rec) const noexcept override
        {
            Bool ret = true;

            $if(!m_bbox.hit(ray, t_min, t_max))
            {
                ret = false;
            }
            $else
            {
                auto hit_left  = m_left->hit(ray, t_min, t_max, time, rec);
                auto hit_right = m_right->hit(ray, t_min, ite(hit_left, rec.t, t_max), time, rec);

                ret = hit_left | hit_right;
            };

            return ret;
        }

        [[nodiscard]] AABB bounding_box() const noexcept override
        {
            return m_bbox;
        }

        static bool box_compare(const luisa::shared_ptr<Hittable>& a, const luisa::shared_ptr<Hittable>& b, int axis) noexcept
        {
            AABB box_a = a->bounding_box();
            AABB box_b = b->bounding_box();

            return box_a.min()[axis] < box_b.min()[axis];
        }

        static bool box_x_compare(const luisa::shared_ptr<Hittable>& a, const luisa::shared_ptr<Hittable>& b) noexcept
        {
            return box_compare(a, b, 0);
        }

        static bool box_y_compare(const luisa::shared_ptr<Hittable>& a, const luisa::shared_ptr<Hittable>& b) noexcept
        {
            return box_compare(a, b, 1);
        }

        static bool box_z_compare(const luisa::shared_ptr<Hittable>& a, const luisa::shared_ptr<Hittable>& b) noexcept
        {
            return box_compare(a, b, 2);
        }
    };
} // namespace Yutrel::RTWeekend