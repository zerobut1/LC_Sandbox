#pragma once

#include <cstddef>
#include <cstdint>

#include <luisa/core/stl/vector.h>

#include "scene/scene_spec.h"

namespace Yutrel
{

class SceneBuilder
{
private:
    enum class BuildState : uint8_t
    {
        Unvisited,
        Visiting,
        Built,
        Failed,
    };

    template <typename Runtime>
    struct BuildCache
    {
        luisa::vector<BuildState> states;
        luisa::vector<const Runtime*> objects;

        explicit BuildCache(size_t size)
            : states(size, BuildState::Unvisited),
              objects(size, nullptr)
        {
        }
    };

private:
    const SceneSpec& _spec;
    BuildCache<Texture> _textures;
    BuildCache<Surface> _surfaces;
    BuildCache<Light> _lights;
    BuildCache<Shape> _shapes;
    BuildCache<Spectrum> _spectra;
    BuildCache<Camera> _cameras;
    BuildCache<Film> _films;
    BuildCache<Filter> _filters;
    BuildCache<Sampler> _samplers;
    BuildCache<Integrator> _integrators;

public:
    explicit SceneBuilder(const SceneSpec& spec) noexcept;

    SceneBuilder()                               = delete;
    SceneBuilder(SceneBuilder&&)                 = delete;
    SceneBuilder& operator=(SceneBuilder&&)      = delete;
    SceneBuilder(const SceneBuilder&)            = delete;
    SceneBuilder& operator=(const SceneBuilder&) = delete;

    [[nodiscard]] const Texture* resolve(TextureRef ref) noexcept;
    [[nodiscard]] const Surface* resolve(SurfaceRef ref) noexcept;
    [[nodiscard]] const Light* resolve(LightRef ref) noexcept;
    [[nodiscard]] const Shape* resolve(ShapeRef ref) noexcept;
    [[nodiscard]] const Spectrum* resolve(SpectrumRef ref) noexcept;
    [[nodiscard]] const Camera* resolve(CameraRef ref) noexcept;
    [[nodiscard]] const Film* resolve(FilmRef ref) noexcept;
    [[nodiscard]] const Filter* resolve(FilterRef ref) noexcept;
    [[nodiscard]] const Sampler* resolve(SamplerRef ref) noexcept;
    [[nodiscard]] const Integrator* resolve(IntegratorRef ref) noexcept;

private:
    template <typename Spec, typename Runtime>
    [[nodiscard]] const Runtime* _resolve(SceneRef<Spec> ref, const SpecTable<Spec>& table, BuildCache<Runtime>& cache) noexcept;
};

} // namespace Yutrel
