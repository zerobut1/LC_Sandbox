// Tests for PBRT scene parsing.
// This test covers parser defaults, parameters, and invalid declarations.

#include "ut/ut.hpp"

#include "pbrt/pbrt_parser.h"

#include <array>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace Yutrel;
using namespace boost::ut;
using namespace boost::ut::literals;

namespace
{

[[nodiscard]] bool is_near(float a, float b) noexcept
{
    return std::abs(a - b) < 1e-4f;
}

[[nodiscard]] const RawParameter* find_parameter(luisa::span<const RawParameter> parameters, luisa::string_view name) noexcept
{
    for (auto&& parameter : parameters)
    {
        if (parameter.name == name)
        {
            return &parameter;
        }
    }
    return nullptr;
}

[[nodiscard]] bool parse_error_contains(
    const std::filesystem::path& path,
    std::initializer_list<std::string_view> expected)
{
    try
    {
        (void)PbrtParser::parse(path);
    }
    catch (const std::runtime_error& error)
    {
        auto message = std::string{error.what()};
        for (auto text : expected)
        {
            if (message.find(text) == std::string::npos)
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

static auto test_pbrt_parse_registration = []
{
    "parse_film_exposure"_test = []
    {
        auto scene = PbrtParser::parse("tests/scenes/film_exposure.pbrt");
        expect(is_near(scene.film.iso, 200.0f));
        expect(is_near(scene.camera.shutter_open, 0.25f));
        expect(is_near(scene.camera.shutter_close, 0.75f));

        auto defaults = PbrtParser::parse("tests/scenes/import_geometry.pbrt");
        expect(is_near(defaults.film.iso, 100.0f));
        expect(is_near(defaults.camera.shutter_open, 0.0f));
        expect(is_near(defaults.camera.shutter_close, 1.0f));
    };

    "parse_sobol_sampler"_test = []
    {
        auto scene = PbrtParser::parse("tests/scenes/sobol_sampler.pbrt");
        expect(scene.sampler.type == SamplerDesc::Type::Sobol);
        expect(scene.sampler.pixel_samples == 32u);
        expect(scene.sampler.seed == 42u);

        auto defaults = PbrtParser::parse("tests/scenes/sobol_sampler_default.pbrt");
        expect(defaults.sampler.type == SamplerDesc::Type::Sobol);
        expect(defaults.sampler.pixel_samples == 16u);
        expect(defaults.sampler.seed == 20120712u);
    };

    "reject_invalid_sobol_sampler"_test = []
    {
        expect(parse_error_contains(
            "tests/scenes/sobol_sampler_zero_spp.pbrt",
            {"sobol_sampler_zero_spp.pbrt", "pixelsamples", "greater than zero"}));
        expect(parse_error_contains(
            "tests/scenes/sobol_sampler_bad_randomization.pbrt",
            {"sobol_sampler_bad_randomization.pbrt", "randomization", "fastowen"}));
    };

    "parse_imagemap_filters"_test = []
    {
        auto scene = PbrtParser::parse("tests/scenes/imagemap_filters.pbrt");
        expect(scene.textures.size() == 3u);
        expect(scene.textures[0u].filter == TextureDesc::Filter::Bilinear);
        expect(scene.textures[1u].filter == TextureDesc::Filter::Point);
        expect(scene.textures[2u].filter == TextureDesc::Filter::Bilinear);
    };

    "parse_checkerboard_textures"_test = []
    {
        auto scene = PbrtParser::parse("tests/scenes/checkerboard_textures.pbrt");
        expect(scene.textures.size() == 4u);

        auto&& floor = scene.textures[0u];
        expect(floor.type == TextureDesc::Type::Checkerboard);
        expect(floor.value_type == TextureDesc::ValueType::Spectrum);
        expect(is_near(floor.uv_scale.x, 20.0f));
        expect(is_near(floor.uv_scale.y, 20.0f));
        expect(!floor.tex1.texture.has_value());
        expect(!floor.tex2.texture.has_value());
        expect(is_near(floor.tex1.constant.x, 0.325f));
        expect(is_near(floor.tex1.constant.y, 0.31f));
        expect(is_near(floor.tex1.constant.z, 0.25f));
        expect(is_near(floor.tex2.constant.x, 0.725f));
        expect(is_near(floor.tex2.constant.y, 0.71f));
        expect(is_near(floor.tex2.constant.z, 0.68f));

        auto&& defaults = scene.textures[1u];
        expect(defaults.type == TextureDesc::Type::Checkerboard);
        expect(defaults.value_type == TextureDesc::ValueType::Float);
        expect(is_near(defaults.uv_scale.x, 1.0f));
        expect(is_near(defaults.uv_scale.y, 1.0f));
        expect(is_near(defaults.tex1.constant.x, 1.0f));
        expect(is_near(defaults.tex2.constant.x, 0.0f));

        auto&& reference = scene.textures[2u];
        expect(reference.type == TextureDesc::Type::Checkerboard);
        expect(reference.tex1.texture == luisa::optional<luisa::string>{"float-source"});
        expect(!reference.tex2.texture.has_value());
        expect(is_near(reference.tex2.constant.x, 0.25f));
    };

    "reject_invalid_checkerboard_parameters"_test = []
    {
        expect(parse_error_contains(
            "tests/scenes/checkerboard_dimension_3.pbrt",
            {"checkerboard_dimension_3.pbrt", "dimension 3", "only 2D"}));
        expect(parse_error_contains(
            "tests/scenes/checkerboard_mapping_spherical.pbrt",
            {"checkerboard_mapping_spherical.pbrt", "mapping 'spherical'", "only 'uv'"}));
        expect(parse_error_contains(
            "tests/scenes/checkerboard_conflicting_tex1.pbrt",
            {"checkerboard_conflicting_tex1.pbrt", "tex1", "both texture and rgb"}));
        expect(parse_error_contains(
            "tests/scenes/checkerboard_duplicate_uscale.pbrt",
            {"checkerboard_duplicate_uscale.pbrt", "duplicate texture parameter", "float uscale"}));
        expect(parse_error_contains(
            "tests/scenes/checkerboard_invalid_input_type.pbrt",
            {"checkerboard_invalid_input_type.pbrt", "unsupported parameter", "float tex1"}));
        expect(parse_error_contains(
            "tests/scenes/checkerboard_nonfinite_scale.pbrt",
            {"checkerboard_nonfinite_scale.pbrt", "UV scale", "finite"}));
    };

    "reject_unsupported_imagemap_filters"_test = []
    {
        struct Case
        {
            const char* path;
            const char* filename;
            const char* expected;
        };
        std::array cases{
            Case{"tests/scenes/imagemap_unsupported_filter.pbrt", "imagemap_unsupported_filter.pbrt", "trilinear"},
            Case{"tests/scenes/imagemap_ewa_filter.pbrt", "imagemap_ewa_filter.pbrt", "ewa"},
            Case{"tests/scenes/imagemap_anisotropic_filter.pbrt", "imagemap_anisotropic_filter.pbrt", "anisotropic"},
            Case{"tests/scenes/imagemap_unknown_filter.pbrt", "imagemap_unknown_filter.pbrt", "unknown"},
        };
        for (auto test_case : cases)
        {
            expect(parse_error_contains(
                test_case.path,
                {test_case.filename, "unsupported imagemap filter", test_case.expected, "point", "bilinear"}));
        }
    };

    "reject_invalid_imagemap_filter_declarations"_test = []
    {
        expect(parse_error_contains(
            "tests/scenes/imagemap_invalid_filter_type.pbrt",
            {"imagemap_invalid_filter_type.pbrt", "unsupported parameter", "integer filter"}));
        expect(parse_error_contains(
            "tests/scenes/imagemap_duplicate_filter.pbrt",
            {"imagemap_duplicate_filter.pbrt", "duplicate texture parameter", "string filter"}));
    };

    "reject_imagemap_without_filename"_test = []
    {
        auto rejected = false;
        try
        {
            (void)PbrtParser::parse("tests/scenes/imagemap_missing_filename.pbrt");
        }
        catch (const std::runtime_error& error)
        {
            auto message = std::string{error.what()};
            rejected = message.find("imagemap_missing_filename.pbrt") != std::string::npos &&
                       message.find("filename") != std::string::npos;
        }
        expect(rejected);
    };

    "reject_conflicting_material_reflectance"_test = []
    {
        auto rejected = false;
        try
        {
            (void)PbrtParser::parse("tests/scenes/material_reflectance_conflict.pbrt");
        }
        catch (const std::runtime_error& error)
        {
            auto message = std::string{error.what()};
            rejected = message.find("material_reflectance_conflict.pbrt") != std::string::npos &&
                       message.find("both rgb and texture") != std::string::npos;
        }
        expect(rejected);
    };

    "parse_sphere_parameters"_test = []
    {
        auto scene = PbrtParser::parse("tests/scenes/sphere_parameters.pbrt");
        expect(scene.shapes.size() == 2u);
        expect(scene.shapes[0u].type == ShapeDesc::Type::Sphere);
        expect(is_near(scene.shapes[0u].radius, 2.5f));
        expect(scene.shapes[0u].sphere_subdivision == 3u);
        expect(scene.shapes[1u].type == ShapeDesc::Type::Sphere);
        expect(is_near(scene.shapes[1u].radius, 1.0f));
        expect(scene.shapes[1u].sphere_subdivision == ShapeDesc::sphere_default_subdivision);
    };

    "parse_coated_diffuse_materials"_test = []
    {
        auto scene = PbrtParser::parse("tests/scenes/coated_diffuse_materials.pbrt");
        expect(scene.materials.size() == 1u);
        expect(scene.named_materials.size() == 1u);

        auto&& inline_material = scene.materials.front();
        expect(inline_material.type == MaterialDesc::Type::CoatedDiffuse);
        expect(is_near(inline_material.reflectance.x, 0.2f));
        expect(is_near(inline_material.reflectance.y, 0.4f));
        expect(is_near(inline_material.reflectance.z, 0.6f));
        expect(is_near(inline_material.roughness, 0.25f));
        expect(is_near(inline_material.u_roughness, 0.25f));
        expect(is_near(inline_material.v_roughness, 0.25f));
        expect(find_parameter(inline_material.parameters, "displacement") != nullptr);

        auto&& named_material = scene.named_materials.at("coated-default");
        expect(named_material.type == MaterialDesc::Type::CoatedDiffuse);
        expect(is_near(named_material.reflectance.x, 0.5f));
        expect(is_near(named_material.reflectance.y, 0.5f));
        expect(is_near(named_material.reflectance.z, 0.5f));
        expect(is_near(named_material.roughness, 0.0f));
        expect(named_material.remap_roughness);
    };

    "reject_invalid_sphere_parameters"_test = []
    {
        for (auto path : {
                 "tests/scenes/sphere_duplicate_radius.pbrt",
                 "tests/scenes/sphere_invalid_radius.pbrt",
                 "tests/scenes/sphere_invalid_subdivision.pbrt",
                 "tests/scenes/sphere_clipped.pbrt",
             })
        {
            auto rejected = false;
            try
            {
                (void)PbrtParser::parse(path);
            }
            catch (const std::runtime_error& error)
            {
                rejected = std::string{error.what()}.find(path) != std::string::npos;
            }
            expect(rejected) << "invalid sphere parameters should produce a source-located parse error";
        }
    };

    "reject_invalid_ply_filenames"_test = []
    {
        for (auto path : {
                 "tests/scenes/ply_missing_filename.pbrt",
                 "tests/scenes/ply_empty_filename.pbrt",
                 "tests/scenes/ply_duplicate_filename.pbrt",
             })
        {
            auto rejected = false;
            try
            {
                (void)PbrtParser::parse(path);
            }
            catch (const std::runtime_error& error)
            {
                auto message = std::string{error.what()};
                rejected     = message.find(path) != std::string::npos;
            }
            expect(rejected) << "invalid plymesh filename should produce a source-located parse error";
        }
    };

    "parse_distant_light"_test = []
    {
        auto scene = PbrtParser::parse("tests/scenes/distant_basic.pbrt");
        expect(scene.distant_light.has_value());
        expect(!scene.infinite_light.has_value());
        if (!scene.distant_light) { return; }
        auto&& light = *scene.distant_light;
        expect(is_near(light.L.x, 8.0f));
        expect(is_near(light.L.y, 8.0f));
        expect(is_near(light.L.z, 8.0f));
        expect(is_near(light.scale, 1.0f));
        expect(is_near(light.from.z, 1.0f));
        expect(is_near(light.to.x, 0.0f));
        expect(is_near(light.pbrt_transform[2u], 1.0f));
        expect(is_near(light.pbrt_transform[8u], -1.0f));
    };

    "reject_invalid_distant_lights"_test = []
    {
        for (auto path : {
                 "tests/scenes/distant_invalid_direction.pbrt",
                 "tests/scenes/distant_invalid_scale.pbrt",
                 "tests/scenes/distant_invalid_radiance.pbrt",
                 "tests/scenes/distant_duplicate.pbrt",
                 "tests/scenes/distant_duplicate_parameter.pbrt",
                 "tests/scenes/distant_with_infinite.pbrt",
             })
        {
            expect(parse_error_contains(path, {path, "LightSource"}));
        }
    };

    "reject_invalid_infinite_lights"_test = []
    {
        for (auto path : {
                 "tests/scenes/infinite_missing_filename.pbrt",
                 "tests/scenes/infinite_invalid_scale.pbrt",
                 "tests/scenes/infinite_duplicate.pbrt",
                 "tests/scenes/infinite_unknown_type.pbrt",
                 "tests/scenes/infinite_unsupported_parameter.pbrt",
             })
        {
            expect(parse_error_contains(path, {path, "LightSource"}));
        }
    };
    return 0;
}();

} // namespace

int main(int argc, char* argv[])
{
    boost::ut::detail::cfg::parse_arg_with_fallback(argc, const_cast<const char**>(argv));
}
