#pragma once

#include <string_view>

#include <luisa/gui/window.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/swapchain.h>

namespace Yutrel
{
    using namespace luisa;
    using namespace luisa::compute;

    struct ApplicationCreateInfo
    {
        std::string_view bin;
        std::string_view backend;
        std::string_view window_name;
        uint2 resolution{1920u, 1080u};
    };

    class Application
    {
    public:
        Application(const ApplicationCreateInfo& info);

        Application() noexcept  = delete;
        ~Application() noexcept = default;

        Application(const Application&)            = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&)                 = default;
        Application& operator=(Application&&)      = default;

        void run();

    private:
        Context m_context;
        Device m_device;
        Stream m_stream;

        string m_window_name;
        uint2 m_resolution;
        Window m_window;
        Swapchain m_swapchain;
    };
} // namespace Yutrel