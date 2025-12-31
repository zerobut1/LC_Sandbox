#pragma once

#include <luisa/gui/window.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <luisa/runtime/swapchain.h>

#include "base/scene.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Renderer;

class Application final
{
public:
    struct CreateInfo
    {
        luisa::string_view bin;
        luisa::string_view backend;
        Scene::CreateInfo scene_info;
    };

private:
    Context m_context;
    Device m_device;
    Stream m_stream;

    luisa::unique_ptr<Scene> m_scene;
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