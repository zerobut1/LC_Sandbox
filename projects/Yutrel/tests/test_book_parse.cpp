// Integration test for complete book.pbrt parsing.
// This test covers scalar parameters, transforms, and attribute inheritance.

#include "ut/ut.hpp"

#include "pbrt/pbrt_parser.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

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

static auto test_book_parse_registration = []
{
    "parse_book_pbrt"_test = []
    {
        auto scene = PbrtParser::parse("scene/pbrt-book/book.pbrt");

        expect(scene.sampler.type == SamplerDesc::Type::Halton);
        expect(scene.sampler.pixel_samples == 2048u);
        expect(scene.textures.size() == 5u);
        expect(scene.materials.size() == 3u);
        expect(scene.shapes.size() == 5u);

        auto sphere_count  = 0u;
        auto plymesh_count = 0u;
        for (auto&& shape : scene.shapes)
        {
            sphere_count += shape.type == ShapeDesc::Type::Sphere ? 1u : 0u;
            plymesh_count += shape.type == ShapeDesc::Type::PlyMesh ? 1u : 0u;
        }
        expect(sphere_count == 2u);
        expect(plymesh_count == 3u);

        auto sensor = find_parameter(scene.film.parameters, "sensor");
        auto iso    = find_parameter(scene.film.parameters, "iso");
        expect(sensor != nullptr);
        expect(iso != nullptr);
        if (sensor == nullptr || iso == nullptr)
        {
            return;
        }
        expect(sensor->values.size() == 1u);
        expect(iso->values.size() == 1u);
        expect(sensor->values.front().quoted);
        expect(sensor->values.front().text == "canon_eos_100d");
        expect(!sensor->bracketed);
        expect(!iso->values.front().quoted);
        expect(iso->values.front().text == "150");
        expect(!iso->bracketed);
        expect(scene.sampler.parameters.front().bracketed);

        expect(scene.shapes[0u].area_light.has_value());
        expect(!scene.shapes[0u].material.inline_index.has_value());
        expect(scene.shapes[2u].material.inline_index == 0u);
        expect(scene.shapes[3u].material.inline_index == 1u);
        expect(scene.shapes[4u].material.inline_index == 2u);
        expect(!scene.shapes[3u].area_light.has_value());

        auto&& camera = scene.camera.pbrt_transform;
        expect(is_near(camera[0u], 1.0f));
        expect(is_near(camera[5u], 1.0f));
        expect(is_near(camera[10u], -1.0f));
        expect(is_near(camera[7u], -2.1088f));
        expect(is_near(camera[11u], 13.574f));

        auto&& translated_sphere = scene.shapes[0u].pbrt_transform;
        expect(is_near(translated_sphere[3u], 34.92f));
        expect(is_near(translated_sphere[7u], 55.92f));
        expect(is_near(translated_sphere[11u], -15.351f));
    };

    "parse_book_v2_ply_filenames"_test = []
    {
        auto scene = PbrtParser::parse("scene/pbrt-book/book-v2.pbrt");
        expect(scene.sampler.type == SamplerDesc::Type::Independent);
        expect(scene.textures.empty());
        expect(scene.materials.size() == 3u);
        for (auto&& material : scene.materials)
        {
            expect(material.type == MaterialDesc::Type::Diffuse);
        }
        expect(scene.shapes.size() == 5u);
        expect(scene.shapes[0u].type == ShapeDesc::Type::Sphere);
        expect(scene.shapes[1u].type == ShapeDesc::Type::Sphere);
        expect(is_near(scene.shapes[0u].radius, 7.5f));
        expect(is_near(scene.shapes[1u].radius, 7.5f));
        expect(scene.shapes[0u].sphere_subdivision == 4u);
        expect(scene.shapes[1u].sphere_subdivision == 4u);

        std::array<std::filesystem::path, 3u> filenames{
            "geometry/mesh_00001.ply",
            "geometry/mesh_00002.ply",
            "geometry/mesh_00003.ply",
        };
        auto filename_index = 0u;
        for (auto&& shape : scene.shapes)
        {
            if (shape.type != ShapeDesc::Type::PlyMesh)
            {
                continue;
            }
            expect(shape.filename.has_value());
            if (shape.filename)
            {
                expect(filename_index < filenames.size());
                if (filename_index < filenames.size())
                {
                    expect(*shape.filename == filenames[filename_index]);
                }
            }
            filename_index++;
        }
        expect(filename_index == filenames.size());
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
    return 0;
}();

} // namespace

int main(int argc, char* argv[])
{
    boost::ut::detail::cfg::parse_arg_with_fallback(argc, const_cast<const char**>(argv));
}
