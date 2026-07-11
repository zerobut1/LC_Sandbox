#include "legacy_scene_adapter.h"

#include "base/film.h"
#include "base/integrator.h"
#include "cameras/pinhole.h"
#include "cameras/thin_lens.h"
#include "filters/box.h"
#include "filters/gaussian.h"
#include "filters/lanczos_sinc.h"
#include "filters/mitchell.h"
#include "filters/triangle.h"
#include "lights/diffuse.h"
#include "samplers/independent.h"
#include "scene/scene_spec_builder.h"
#include "shapes/inline_mesh.h"
#include "shapes/mesh.h"
#include "spectrum/hero.h"
#include "spectrum/srgb.h"
#include "surfaces/diffuse.h"
#include "surfaces/null.h"
#include "textures/checker_board.h"
#include "textures/constant.h"
#include "textures/image.h"

namespace Yutrel
{
namespace
{
[[nodiscard]] SourceLocation legacy_source()
{
    return SourceLocation{.file = "legacy-create-info"};
}

[[nodiscard]] TextureRef add_texture(SceneSpecBuilder& builder, const Texture::CreateInfo& info)
{
    auto source = legacy_source();
    switch (info.type)
    {
    case Texture::Type::constant:
        return builder.add_anonymous_texture<ConstantTextureSpec>(source, info.v);
    case Texture::Type::checker_board:
    {
        auto even = builder.add_anonymous_texture<ConstantTextureSpec>(source, info.even);
        auto odd  = builder.add_anonymous_texture<ConstantTextureSpec>(source, info.odd);
        return builder.add_anonymous_texture<CheckerBoardTextureSpec>(source, info.scale, even, odd);
    }
    case Texture::Type::image:
        return builder.add_anonymous_texture<ImageTextureSpec>(source, info.path, info.sampler, info.encoding);
    }
    LUISA_ERROR("Unsupported legacy texture type {}.", static_cast<uint>(info.type));
}

[[nodiscard]] SurfaceRef add_surface(SceneSpecBuilder& builder, const Surface::CreateInfo& info)
{
    auto source = legacy_source();
    switch (info.type)
    {
    case Surface::Type::null:
        return builder.add_anonymous_surface<NullSurfaceSpec>(source, info.two_sided);
    case Surface::Type::diffuse:
        return builder.add_anonymous_surface<DiffuseSurfaceSpec>(source, add_texture(builder, info.reflectance), info.two_sided);
    }
    LUISA_ERROR("Unsupported legacy surface type {}.", static_cast<uint>(info.type));
}

[[nodiscard]] luisa::optional<LightRef> add_light(SceneSpecBuilder& builder, const Light::CreateInfo& info)
{
    switch (info.type)
    {
    case Light::Type::null:
        return luisa::nullopt;
    case Light::Type::diffuse:
        return builder.add_anonymous_light<DiffuseLightSpec>(legacy_source(), add_texture(builder, info.emission), info.scale, info.two_sided);
    }
    LUISA_ERROR("Unsupported legacy light type {}.", static_cast<uint>(info.type));
}

[[nodiscard]] ShapeRef add_shape(SceneSpecBuilder& builder, const Shape::CreateInfo& info)
{
    auto source = legacy_source();
    switch (info.type)
    {
    case Shape::Type::mesh:
        return builder.add_anonymous_shape<MeshShapeSpec>(source, info.path);
    case Shape::Type::inline_mesh:
        return builder.add_anonymous_shape<InlineMeshShapeSpec>(source, info.positions, info.normals, info.uvs, info.indices);
    }
    LUISA_ERROR("Unsupported legacy shape type {}.", static_cast<uint>(info.type));
}

[[nodiscard]] FilterRef add_filter(SceneSpecBuilder& builder, const Filter::CreateInfo& info)
{
    auto source = legacy_source();
    switch (info.type)
    {
    case Filter::Type::Box:
        return builder.add_anonymous_filter<BoxFilterSpec>(source, info.radius);
    case Filter::Type::Triangle:
        return builder.add_anonymous_filter<TriangleFilterSpec>(source, info.radius);
    case Filter::Type::Gaussian:
        return builder.add_anonymous_filter<GaussianFilterSpec>(source, info.radius);
    case Filter::Type::Mitchell:
        return builder.add_anonymous_filter<MitchellFilterSpec>(source, info.radius);
    case Filter::Type::LanczosSinc:
        return builder.add_anonymous_filter<LanczosSincFilterSpec>(source, info.radius);
    }
    LUISA_ERROR("Unsupported legacy filter type {}.", static_cast<uint>(info.type));
}

[[nodiscard]] SpectrumRef add_spectrum(SceneSpecBuilder& builder, Spectrum::Type type)
{
    switch (type)
    {
    case Spectrum::Type::SRGB:
        return builder.add_anonymous_spectrum<SRGBSpectrumSpec>(legacy_source());
    case Spectrum::Type::HeroWavelength:
        return builder.add_anonymous_spectrum<HeroWavelengthSpectrumSpec>(legacy_source());
    }
    LUISA_ERROR("Unsupported legacy spectrum type {}.", static_cast<uint>(type));
}

[[nodiscard]] CameraRef add_camera(SceneSpecBuilder& builder, const Camera::CreateInfo& info)
{
    switch (info.type)
    {
    case Camera::Type::pinhole:
        return builder.add_anonymous_camera<PinholeCameraSpec>(legacy_source(), info.position, info.lookat, info.up, info.shutter_span, info.shutter_samples_count, info.fov);
    case Camera::Type::thin_lens:
        return builder.add_anonymous_camera<ThinLensCameraSpec>(legacy_source(), info.position, info.lookat, info.up, info.shutter_span, info.shutter_samples_count, info.aperture, info.focal_length, info.focus_distance);
    }
    LUISA_ERROR("Unsupported legacy camera type {}.", static_cast<uint>(info.type));
}
} // namespace

SceneSpec make_legacy_scene_spec(const Scene::CreateInfo& info)
{
    SceneSpecBuilder builder;
    auto source = legacy_source();

    auto spectrum     = add_spectrum(builder, info.spectrum_info.type);
    auto& camera_info = info.camera_info;
    auto camera       = add_camera(builder, camera_info);
    auto film         = builder.add_anonymous_film<RGBFilmSpec>(source, camera_info.film_info.resolution, camera_info.film_info.display_hdr, camera_info.film_info.filename);
    auto filter       = add_filter(builder, camera_info.filter_info);
    auto sampler      = builder.add_anonymous_sampler<IndependentSamplerSpec>(source, camera_info.spp, 20120712u);
    auto integrator   = builder.add_anonymous_integrator<PathIntegratorSpec>(source, info.integrator_info.max_depth, info.integrator_info.rr_depth, info.integrator_info.rr_threshold);

    for (auto& shape_info : info.shape_infos)
    {
        builder.add_instance(ShapeInstanceSpec{
            .source  = source,
            .shape   = add_shape(builder, shape_info),
            .surface = add_surface(builder, shape_info.surface_info),
            .light   = add_light(builder, shape_info.light_info),
        });
    }
    builder.set_render(RenderSpec{
        .spectrum   = spectrum,
        .camera     = camera,
        .film       = film,
        .filter     = filter,
        .sampler    = sampler,
        .integrator = integrator,
    });
    return builder.finish();
}
} // namespace Yutrel
