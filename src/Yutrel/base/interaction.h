#pragma once

#include <luisa/dsl/syntax.h>

#include "base/shape.h"
#include "utils/frame.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Interaction
{
public:
    Shape::Handle shape;
    Float3 p_g;
    Float3 n_g;
    Float2 uv;
    Float3 p_s;
    Frame shading;
    UInt inst_id;
    UInt prim_id;
    Float prim_area;
    Bool front_face;

public:
    [[nodiscard]] auto valid() const noexcept { return inst_id != ~0u; }

public:
    static constexpr auto default_t_max = std::numeric_limits<float>::max();
    [[nodiscard]] Var<Ray> spawn_ray(Expr<float3> wi, Expr<float> t_max = default_t_max) const noexcept
    {
        return make_ray(p_s + shading.n() * 1e-4f, wi);
    }
};
} // namespace Yutrel