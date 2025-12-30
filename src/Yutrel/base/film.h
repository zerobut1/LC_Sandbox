#pragma once

#include <imgui.h>
#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>
#include <luisa/gui/framerate.h>
#include <luisa/gui/imgui_window.h>
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
    public:
        struct CreateInfo
        {
            uint2 resolution{1920u, 1080u};
        };

        [[nodiscard]] static luisa::unique_ptr<Film> create(const CreateInfo& info) noexcept;

    public:
        class Instance
        {
        private:
            const Renderer& m_renderer;
            const Film* m_film;

            // render image
            mutable Buffer<float4> m_image;
            Shader1D<Buffer<float4>> m_clear_image;

            // window display
            Stream* m_stream{};
            luisa::unique_ptr<ImGuiWindow> m_window;
            Image<float> m_framebuffer;
            ImTextureID m_background{};
            Shader2D<> m_blit;
            Shader2D<Image<float>> m_clear;
            bool m_rendering_finished{false};
            mutable Framerate m_framerate{};

        public:
            explicit Instance(const Renderer& renderer, const Film* film) noexcept
                : m_renderer(renderer), m_film(film) {}
            ~Instance() noexcept = default;

            Instance()                           = delete;
            Instance(const Instance&)            = delete;
            Instance& operator=(const Instance&) = delete;
            Instance(Instance&&)                 = delete;
            Instance& operator=(Instance&&)      = delete;

            template <typename T = Film>
                requires std::is_base_of_v<Film, T>
            [[nodiscard]] auto base() const noexcept
            {
                return static_cast<const T*>(m_film);
            }

        public:
            [[nodiscard]] auto& renderer() const noexcept { return m_renderer; }

            void accumulate(Expr<uint2> pixel, Expr<float3> rgb, Expr<float> effective_spp) const noexcept;

            void prepare(CommandBuffer& command_buffer) noexcept;
            void release() noexcept;
            bool show(CommandBuffer& command_buffer) const noexcept;

        private:
            void display() const noexcept;
        };

    private:
        uint2 m_resolution{1920u, 1080u};

    public:
        explicit Film(const CreateInfo& info) noexcept;
        ~Film() noexcept;

        Film()                       = delete;
        Film(const Film&)            = delete;
        Film& operator=(const Film&) = delete;
        Film(Film&&)                 = delete;
        Film& operator=(Film&&)      = delete;

    public:
        [[nodiscard]] luisa::unique_ptr<Instance> build(const Renderer& renderer, CommandBuffer& command_buffer) const noexcept
        {
            return luisa::make_unique<Instance>(renderer, this);
        }

        [[nodiscard]] uint2 resolution() const noexcept { return m_resolution; }
    };
} // namespace Yutrel