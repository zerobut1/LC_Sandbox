#pragma once

#include <utility>

#include "scene/spec_base.h"
#include "scene/spec_table.h"

namespace Yutrel
{

class SceneSpecBuilder;

class SceneSpec
{
private:
    SpecTable<TextureSpec> _textures;
    SpecTable<SurfaceSpec> _surfaces;
    SpecTable<LightSpec> _lights;
    SpecTable<ShapeSpec> _shapes;
    SpecTable<SpectrumSpec> _spectra;
    SpecTable<CameraSpec> _cameras;
    SpecTable<FilmSpec> _films;
    SpecTable<FilterSpec> _filters;
    SpecTable<SamplerSpec> _samplers;
    SpecTable<IntegratorSpec> _integrators;

private:
    SceneSpec(
        SpecTable<TextureSpec> textures,
        SpecTable<SurfaceSpec> surfaces,
        SpecTable<LightSpec> lights,
        SpecTable<ShapeSpec> shapes,
        SpecTable<SpectrumSpec> spectra,
        SpecTable<CameraSpec> cameras,
        SpecTable<FilmSpec> films,
        SpecTable<FilterSpec> filters,
        SpecTable<SamplerSpec> samplers,
        SpecTable<IntegratorSpec> integrators) noexcept
        : _textures{std::move(textures)},
          _surfaces{std::move(surfaces)},
          _lights{std::move(lights)},
          _shapes{std::move(shapes)},
          _spectra{std::move(spectra)},
          _cameras{std::move(cameras)},
          _films{std::move(films)},
          _filters{std::move(filters)},
          _samplers{std::move(samplers)},
          _integrators{std::move(integrators)}
    {
    }

    friend class SceneSpecBuilder;

public:
    SceneSpec()                                = delete;
    SceneSpec(SceneSpec&&) noexcept            = default;
    SceneSpec& operator=(SceneSpec&&) noexcept = default;
    SceneSpec(const SceneSpec&)                = delete;
    SceneSpec& operator=(const SceneSpec&)     = delete;
    ~SceneSpec() noexcept                      = default;

    [[nodiscard]] const SpecTable<TextureSpec>& textures() const noexcept { return _textures; }
    [[nodiscard]] const SpecTable<SurfaceSpec>& surfaces() const noexcept { return _surfaces; }
    [[nodiscard]] const SpecTable<LightSpec>& lights() const noexcept { return _lights; }
    [[nodiscard]] const SpecTable<ShapeSpec>& shapes() const noexcept { return _shapes; }
    [[nodiscard]] const SpecTable<SpectrumSpec>& spectra() const noexcept { return _spectra; }
    [[nodiscard]] const SpecTable<CameraSpec>& cameras() const noexcept { return _cameras; }
    [[nodiscard]] const SpecTable<FilmSpec>& films() const noexcept { return _films; }
    [[nodiscard]] const SpecTable<FilterSpec>& filters() const noexcept { return _filters; }
    [[nodiscard]] const SpecTable<SamplerSpec>& samplers() const noexcept { return _samplers; }
    [[nodiscard]] const SpecTable<IntegratorSpec>& integrators() const noexcept { return _integrators; }
};

} // namespace Yutrel
