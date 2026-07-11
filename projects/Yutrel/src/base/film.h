#pragma once

#include <filesystem>

#include <imgui.h>
#include <luisa/core/stl/memory.h>
#include <luisa/dsl/syntax.h>
#include <luisa/gui/framerate.h>
#include <luisa/gui/imgui_window.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/swapchain.h>

#include "scene/spec_base.h"
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
        bool display_hdr{false};
        std::filesystem::path filename{"render.exr"};
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
        mutable Buffer<float4> m_converted;
        Shader1D<Buffer<float4>> m_clear_image;
        Shader1D<Buffer<float4>, Buffer<float4>> m_convert_image;

        // window display
        Stream* m_stream{};
        luisa::unique_ptr<ImGuiWindow> m_window;
        Image<float> m_framebuffer;
        ImTextureID m_background{};
        Shader2D<bool> m_blit;
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

        [[nodiscard]] bool should_close() const noexcept;

        void accumulate(Expr<uint2> pixel, Expr<float3> rgb, Expr<float> effective_spp) const noexcept;
        // Requires at most one invocation to write each pixel in a dispatch, with
        // consecutive dispatches ordered on the same stream.
        void accumulate_single_writer(Expr<uint2> pixel, Expr<float3> rgb, Expr<float> effective_spp) const noexcept;

        void prepare(CommandBuffer& command_buffer, bool enable_display) noexcept;
        void download(CommandBuffer& command_buffer, float4* buffer) const noexcept;
        void release() noexcept;
        bool show(CommandBuffer& command_buffer, bool force = false) const noexcept;

    private:
        [[nodiscard]] Float4 filtered_contribution(Expr<float3> rgb, Expr<float> effective_spp) const noexcept;
        void display() const noexcept;
    };

private:
    uint2 m_resolution{1920u, 1080u};
    bool m_display_hdr{false};
    std::filesystem::path m_filename{"render.exr"};

public:
    Film(uint2 resolution, bool display_hdr, std::filesystem::path filename) noexcept;
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

    [[nodiscard]] auto resolution() const noexcept { return m_resolution; }
    [[nodiscard]] auto display_hdr() const noexcept { return m_display_hdr; }
    [[nodiscard]] const auto& filename() const noexcept { return m_filename; }
};

class RGBFilmSpec final : public FilmSpec
{
private:
    uint2 _resolution;
    bool _display_hdr;
    std::filesystem::path _filename;

public:
    RGBFilmSpec(uint2 resolution, bool display_hdr, std::filesystem::path filename) noexcept
        : _resolution{resolution}, _display_hdr{display_hdr}, _filename{std::move(filename)} {}

    [[nodiscard]] luisa::optional<luisa::string> validate() const noexcept override
    {
        if (_resolution.x == 0u || _resolution.y == 0u)
        {
            return spec_validation_error("Film resolution must be non-zero.");
        }
        if (_filename.empty())
        {
            return spec_validation_error("Film output path cannot be empty.");
        }
        return luisa::nullopt;
    }
    [[nodiscard]] const Film* build(SceneBuilder& builder) const noexcept override;
};
} // namespace Yutrel
