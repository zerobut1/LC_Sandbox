#pragma once

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/film.h"
#include "base/shape.h"
#include "base/surface.h"
#include "base/texture.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class Scene
{
public:
    struct CreateInfo
    {
        Camera::CreateInfo camera_info;
        luisa::vector<Shape::CreateInfo> shape_infos;
    };

    struct Config;

private:
    const Context& m_context;
    luisa::unique_ptr<Config> m_config;

public:
    explicit Scene(const Context& context) noexcept;
    ~Scene() noexcept;

    Scene() noexcept               = delete;
    Scene(const Scene&)            = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&)                 = delete;
    Scene& operator=(Scene&&)      = delete;

public:
    [[nodiscard]] static luisa::unique_ptr<Scene> create(const Context& context, const CreateInfo& info) noexcept;

    void load_camera(const Camera::CreateInfo& info) noexcept;
    [[nodiscard]] const Film* load_film(const Film::CreateInfo& info) noexcept;
    [[nodiscard]] const Shape* load_shape(const Shape::CreateInfo& info) noexcept;
    [[nodiscard]] const Surface* load_surface(const Surface::CreateInfo& info) noexcept;
    [[nodiscard]] const Texture* load_texture(const Texture::CreateInfo& info) noexcept;

    [[nodiscard]] const Camera* camera() const noexcept;
    [[nodiscard]] const Film* film() const noexcept;
    [[nodiscard]] luisa::span<const Shape* const> shapes() const noexcept;
};

} // namespace Yutrel