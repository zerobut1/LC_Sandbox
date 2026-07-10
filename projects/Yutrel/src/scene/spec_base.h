#pragma once

#include "scene/scene_ref.h"

namespace Yutrel
{

class SceneBuilder;

class Texture;
class Surface;
class Light;
class Shape;
class Spectrum;
class Camera;
class Film;
class Filter;
class Sampler;
class Integrator;

class SpecDependencyVisitor
{
public:
    virtual ~SpecDependencyVisitor() noexcept = default;

    virtual void visit(TextureRef ref) noexcept    = 0;
    virtual void visit(SurfaceRef ref) noexcept    = 0;
    virtual void visit(LightRef ref) noexcept      = 0;
    virtual void visit(ShapeRef ref) noexcept      = 0;
    virtual void visit(SpectrumRef ref) noexcept   = 0;
    virtual void visit(CameraRef ref) noexcept     = 0;
    virtual void visit(FilmRef ref) noexcept       = 0;
    virtual void visit(FilterRef ref) noexcept     = 0;
    virtual void visit(SamplerRef ref) noexcept    = 0;
    virtual void visit(IntegratorRef ref) noexcept = 0;
};

class TextureSpec
{
public:
    virtual ~TextureSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Texture* build(SceneBuilder& builder) const noexcept = 0;
};

class SurfaceSpec
{
public:
    virtual ~SurfaceSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Surface* build(SceneBuilder& builder) const noexcept = 0;
};

class LightSpec
{
public:
    virtual ~LightSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Light* build(SceneBuilder& builder) const noexcept = 0;
};

class ShapeSpec
{
public:
    virtual ~ShapeSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Shape* build(SceneBuilder& builder) const noexcept = 0;
};

class SpectrumSpec
{
public:
    virtual ~SpectrumSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Spectrum* build(SceneBuilder& builder) const noexcept = 0;
};

class CameraSpec
{
public:
    virtual ~CameraSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Camera* build(SceneBuilder& builder) const noexcept = 0;
};

class FilmSpec
{
public:
    virtual ~FilmSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Film* build(SceneBuilder& builder) const noexcept = 0;
};

class FilterSpec
{
public:
    virtual ~FilterSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Filter* build(SceneBuilder& builder) const noexcept = 0;
};

class SamplerSpec
{
public:
    virtual ~SamplerSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Sampler* build(SceneBuilder& builder) const noexcept = 0;
};

class IntegratorSpec
{
public:
    virtual ~IntegratorSpec() noexcept = default;
    virtual void visit_dependencies(SpecDependencyVisitor&) const noexcept {}
    [[nodiscard]] virtual const Integrator* build(SceneBuilder& builder) const noexcept = 0;
};

} // namespace Yutrel
