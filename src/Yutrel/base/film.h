#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>
#include <luisa/gui/framerate.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/swapchain.h>

#include "utils/command_buffer.h"

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Renderer;
    class Camera;

    class Film
    {
    private:
        const Renderer* m_renderer;
        Stream* m_stream;

        luisa::unique_ptr<Window> m_window;
        Swapchain m_swapchain;
        Image<float> m_framebuffer;
        mutable Framerate m_framerate{};

        bool m_rendering_finished{false};
        uint2 m_resolution{1920u, 1080u};

    public:
        explicit Film(Renderer& renderer) noexcept;
        ~Film() noexcept;

        Film() noexcept              = delete;
        Film(const Film&)            = delete;
        Film& operator=(const Film&) = delete;
        Film(Film&&)                 = delete;
        Film& operator=(Film&&)      = delete;

    public:
        [[nodiscard]] static luisa::unique_ptr<Film> create(Renderer& renderer) noexcept;

        [[nodiscard]] uint2 resolution() const noexcept { return m_resolution; }

        void accumulate(UInt2 pixel_id, Float3 rgb) const noexcept;

        void prepare(CommandBuffer& command_buffer) noexcept;
        void release() noexcept;
        bool show(CommandBuffer& command_buffer) const noexcept;
    };
} // namespace Yutrel