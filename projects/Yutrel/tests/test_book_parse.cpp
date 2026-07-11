// Integration test for complete book.pbrt parsing.
// This test covers scalar parameters, transforms, and attribute inheritance.

#include "ut/ut.hpp"

#include "pbrt/pbrt_parser.h"

#include <cmath>

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
    return 0;
}();

} // namespace

int main(int argc, char* argv[])
{
    boost::ut::detail::cfg::parse_arg_with_fallback(argc, const_cast<const char**>(argv));
}
