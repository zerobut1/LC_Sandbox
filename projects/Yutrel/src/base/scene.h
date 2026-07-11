#pragma once

#include <luisa/luisa-compute.h>

#include "base/camera.h"
#include "base/film.h"
#include "base/integrator.h"
#include "base/shape.h"
#include "base/spectrum.h"
#include "base/surface.h"
#include "base/texture.h"

namespace Yutrel
{
using namespace luisa;
using namespace luisa::compute;

class SceneBuilder;
class SceneSpec;

class Scene
{
public:
    struct CreateInfo
    {
        Spectrum::CreateInfo spectrum_info;
        Integrator::CreateInfo integrator_info;
        Camera::CreateInfo camera_info;
        luisa::vector<Shape::CreateInfo> shape_infos;
    };

    struct Config;

private:
    const Context& m_context;
    luisa::unique_ptr<Config> m_config;

    friend class SceneBuilder;

private:
    [[nodiscard]] const Texture* _store(luisa::unique_ptr<Texture> texture) noexcept;
    [[nodiscard]] const Surface* _store(luisa::unique_ptr<Surface> surface) noexcept;
    [[nodiscard]] const Light* _store(luisa::unique_ptr<Light> light) noexcept;
    [[nodiscard]] const Shape* _store(luisa::unique_ptr<Shape> shape) noexcept;
    [[nodiscard]] const Spectrum* _store(luisa::unique_ptr<Spectrum> spectrum) noexcept;
    [[nodiscard]] const Camera* _store(luisa::unique_ptr<Camera> camera) noexcept;
    [[nodiscard]] const Film* _store(luisa::unique_ptr<Film> film) noexcept;
    [[nodiscard]] const Filter* _store(luisa::unique_ptr<Filter> filter) noexcept;
    [[nodiscard]] const Sampler* _store(luisa::unique_ptr<Sampler> sampler) noexcept;
    [[nodiscard]] const Integrator* _store(luisa::unique_ptr<Integrator> integrator) noexcept;
    void _set_render_roots(const Spectrum* spectrum, const Camera* camera, const Film* film, const Filter* filter, const Sampler* sampler, const Integrator* integrator) noexcept;
    void _add_instance(ShapeInstance instance) noexcept;

public:
    explicit Scene(const Context& context) noexcept;
    ~Scene() noexcept;

    Scene() noexcept               = delete;
    Scene(const Scene&)            = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&)                 = delete;
    Scene& operator=(Scene&&)      = delete;

    [[nodiscard]] static luisa::unique_ptr<Scene> create(const Context& context, const CreateInfo& info);
    [[nodiscard]] static luisa::unique_ptr<Scene> create(const Context& context, const SceneSpec& spec) noexcept;

    [[nodiscard]] const Texture* load_texture(const Texture::CreateInfo& info) noexcept;
    [[nodiscard]] const Surface* load_surface(const Surface::CreateInfo& info) noexcept;
    [[nodiscard]] const Light* load_light(const Light::CreateInfo& info) noexcept;

    [[nodiscard]] const Spectrum* spectrum() const noexcept;
    [[nodiscard]] const Camera* camera() const noexcept;
    [[nodiscard]] const Film* film() const noexcept;
    [[nodiscard]] const Filter* filter() const noexcept;
    [[nodiscard]] const Sampler* sampler() const noexcept;
    [[nodiscard]] const Integrator* integrator() const noexcept;
    [[nodiscard]] luisa::span<const ShapeInstance> instances() const noexcept;
};
} // namespace Yutrel
