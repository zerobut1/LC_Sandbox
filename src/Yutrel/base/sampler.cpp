#include "sampler.h"

#include <luisa/luisa-compute.h>

#include "base/renderer.h"
#include "utils/rng.h"

namespace Yutrel
{
    Sampler::Sampler(const Renderer& renderer) noexcept
        : m_renderer(renderer) {}

    luisa::unique_ptr<Sampler> Sampler::create(const Renderer& renderer) noexcept
    {
        return luisa::make_unique<Sampler>(renderer);
    }

    void Sampler::reset(CommandBuffer& command_buffer, uint state_count) noexcept
    {
        if (!m_states || state_count > m_states.size())
        {
            m_states = m_renderer.device().create_buffer<uint>(next_pow2(state_count));
        }
    }

    void Sampler::start(UInt2 pixel, UInt index) noexcept
    {
        m_state.emplace(xxhash32(make_uint4(pixel, seed(), index)));
    }

    [[nodiscard]] Float Sampler::generate_1d() noexcept
    {
        Float u = 0.0f;
        $outline { u = lcg(*m_state); };
        return u;
    }

    [[nodiscard]] Float2 Sampler::generate_2d() noexcept
    {
        Float2 u = make_float2();
        $outline
        {
            u.x = generate_1d();
            u.y = generate_1d();
        };
        return u;
    }

} // namespace Yutrel