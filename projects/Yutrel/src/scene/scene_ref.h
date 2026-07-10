#pragma once

#include <compare>
#include <cstdint>

namespace Yutrel
{

class TextureSpec;
class SurfaceSpec;
class LightSpec;
class ShapeSpec;
class SpectrumSpec;
class CameraSpec;
class FilmSpec;
class FilterSpec;
class SamplerSpec;
class IntegratorSpec;

template <typename Spec>
class SpecTable;

template <typename Spec>
class SceneRef
{
private:
    uint32_t _index;

private:
    explicit SceneRef(uint32_t index) noexcept
        : _index{index}
    {
    }

    friend class SpecTable<Spec>;

public:
    SceneRef() = delete;

    [[nodiscard]] uint32_t index() const noexcept { return _index; }
    auto operator<=>(const SceneRef&) const noexcept = default;
};

using TextureRef    = SceneRef<TextureSpec>;
using SurfaceRef    = SceneRef<SurfaceSpec>;
using LightRef      = SceneRef<LightSpec>;
using ShapeRef      = SceneRef<ShapeSpec>;
using SpectrumRef   = SceneRef<SpectrumSpec>;
using CameraRef     = SceneRef<CameraSpec>;
using FilmRef       = SceneRef<FilmSpec>;
using FilterRef     = SceneRef<FilterSpec>;
using SamplerRef    = SceneRef<SamplerSpec>;
using IntegratorRef = SceneRef<IntegratorSpec>;

} // namespace Yutrel
