#pragma once

#include "base/light.h"

namespace Yutrel
{
class NullLight : public Light
{
public:
    explicit NullLight(Scene& scene, const CreateInfo& info) noexcept : Light(scene, info) {}

    [[nodiscard]] bool is_null() const noexcept override { return true; }
    [[nodiscard]] luisa::unique_ptr<Instance> build(Renderer& renderer, CommandBuffer& command_buffer) const noexcept override
    {
        return nullptr;
    }
};

} // namespace Yutrel