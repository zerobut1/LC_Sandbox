#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>

#include "utils/command_buffer.h"

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Renderer;

    class Sampler
    {
    private:
        const Renderer* m_renderer;

        uint m_seed{20120712u};
        Buffer<uint> m_states;
        luisa::optional<Var<uint>> m_state;

    public:
        explicit Sampler(const Renderer* renderer) noexcept;
        ~Sampler() noexcept = default;

        Sampler(const Sampler&) noexcept            = delete;
        Sampler(Sampler&&) noexcept                 = delete;
        Sampler& operator=(const Sampler&) noexcept = delete;
        Sampler& operator=(Sampler&&) noexcept      = delete;

    public:
        [[nodiscard]] static luisa::unique_ptr<Sampler> create(const Renderer* renderer) noexcept;
        [[nodiscard]] auto seed() const noexcept { return m_seed; }

        void reset(CommandBuffer& command_buffer, uint state_count) noexcept;

        void start(UInt2 pixel, UInt index) noexcept;
        [[nodiscard]] Float generate_1d() noexcept;
        [[nodiscard]] Float2 generate_2d() noexcept;
    };
} // namespace Yutrel