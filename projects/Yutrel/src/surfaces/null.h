#pragma once

#include "base/surface.h"
#include "scene/scene_builder.h"

namespace Yutrel
{
class NullSurface : public Surface
{
public:
    explicit NullSurface(bool two_sided = false) noexcept : Surface{two_sided} {}

    [[nodiscard]] bool is_null() const noexcept override { return true; }
    [[nodiscard]] luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override
    {
        LUISA_ERROR_WITH_LOCATION("NullSurface cannot be instantiated.");
        return nullptr;
    }
};

class NullSurfaceSpec final : public SurfaceSpec
{
private:
    bool _two_sided;

public:
    explicit NullSurfaceSpec(bool two_sided = false) noexcept
        : _two_sided{two_sided} {}

    [[nodiscard]] const Surface* build(SceneBuilder& builder) const noexcept override
    {
        return builder.emplace<Surface, NullSurface>(_two_sided);
    }
};
} // namespace Yutrel
