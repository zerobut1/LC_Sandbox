#pragma once

#include <luisa/gui/window.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/swapchain.h>

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    class Renderer;

    class Application
    {
    public:
        struct CreateInfo
        {
            luisa::string_view bin;
            luisa::string_view backend;
            luisa::string_view window_name;
            uint2 resolution{1920u, 1080u};
        };

    private:
        Context m_context;
        Device m_device;
        Stream m_stream;

        luisa::unique_ptr<Renderer> m_renderer;

    public:
        explicit Application(const CreateInfo& info);
        ~Application() noexcept;

        Application() noexcept                     = delete;
        Application(const Application&)            = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&)                 = delete;
        Application& operator=(Application&&)      = delete;

    public:
        void run();
    };
} // namespace Yutrel