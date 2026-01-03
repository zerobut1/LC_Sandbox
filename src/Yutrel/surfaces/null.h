#pragma once

#include "base/surface.h"

namespace Yutrel
{
class NullSurface : public Surface
{
public:
    explicit NullSurface(Scene& scene, Surface::CreateInfo info) noexcept : Surface(scene, info) {}

    [[nodiscard]] bool is_null() const noexcept override { return true; }
    [[nodiscard]] luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override
    {
        LUISA_ERROR_WITH_LOCATION("NullSurface cannot be instantiated.");
        return nullptr;
    }
};
} // namespace Yutrel