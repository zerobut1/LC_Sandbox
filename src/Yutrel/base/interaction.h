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
};
} // namespace Yutrel