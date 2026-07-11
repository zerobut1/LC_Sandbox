#include "ut/ut.hpp"

#include "base/shape.h"
#include "pbrt/pbrt_importer.h"
#include "pbrt/pbrt_parser.h"
#include "shapes/mesh.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

using namespace Yutrel;
using namespace boost::ut;
using namespace boost::ut::literals;

namespace
{

[[nodiscard]] bool is_near(float a, float b, float epsilon = 1e-4f) noexcept
{
    return std::abs(a - b) < epsilon;
}

[[nodiscard]] float column_length(const luisa::float4x4& m, uint column) noexcept
{
    auto v = m[column];
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static auto test_book_import_registration = []
{
    "import_book_ply_geometry"_test = []
    {
        auto parsed = PbrtParser::parse("tests/scenes/book_geometry.pbrt");
        std::array<Matrix4, 3u> expected_transforms{
            parsed.shapes[0u].pbrt_transform,
            parsed.shapes[1u].pbrt_transform,
            parsed.shapes[2u].pbrt_transform,
        };
        auto spec = PbrtImporter::import(std::move(parsed));

        expect(spec.textures().size() == 3u);
        expect(spec.surfaces().size() == 3u);
        expect(spec.shapes().size() == 3u);
        expect(spec.instances().size() == 3u);

        auto instances = spec.instances();
        expect(instances[0u].surface != instances[1u].surface);
        expect(instances[0u].surface != instances[2u].surface);
        expect(instances[1u].surface != instances[2u].surface);

        std::array<std::filesystem::path, 3u> expected_paths{
            std::filesystem::absolute("scene/pbrt-book/geometry/mesh_00001.ply"),
            std::filesystem::absolute("scene/pbrt-book/geometry/mesh_00002.ply"),
            std::filesystem::absolute("scene/pbrt-book/geometry/mesh_00003.ply"),
        };
        auto shape_index = 0u;
        spec.shapes().visit_entries([&](ShapeRef, const SpecMeta&, const ShapeSpec* shape)
        {
            auto mesh = dynamic_cast<const MeshShapeSpec*>(shape);
            expect(mesh != nullptr);
            if (mesh != nullptr)
            {
                expect(shape_index < expected_paths.size());
                if (shape_index < expected_paths.size())
                {
                    expect(mesh->path().lexically_normal() == expected_paths[shape_index].lexically_normal());
                }
            }
            shape_index++;
        });
        expect(shape_index == expected_paths.size());

        for (auto instance_index = 0u; instance_index < instances.size(); instance_index++)
        {
            for (auto column = 0u; column < 4u; column++)
            {
                for (auto row = 0u; row < 4u; row++)
                {
                    expect(is_near(
                        instances[instance_index].transform[column][row],
                        expected_transforms[instance_index][row * 4u + column]));
                }
            }
        }
        auto&& first    = instances[0u].transform;
        expect(is_near(first[0u].x, 0.213f));
        expect(is_near(first[1u].y, 0.213f));
        expect(is_near(first[2u].z, 0.213f));

        for (auto instance_index : {1u, 2u})
        {
            auto&& transform = instances[instance_index].transform;
            expect(is_near(transform[3u].x, 0.0f));
            expect(is_near(transform[3u].y, 2.2f));
            expect(is_near(transform[3u].z, 0.0f));
            expect(is_near(column_length(transform, 0u), 0.5f));
            expect(is_near(column_length(transform, 1u), 0.5f));
            expect(is_near(column_length(transform, 2u), 0.5f));
        }
    };

    "reuse_inherited_inline_material"_test = []
    {
        auto parsed = PbrtParser::parse("tests/scenes/book_geometry.pbrt");
        parsed.shapes[1u].material.inline_index = parsed.shapes[0u].material.inline_index;
        auto spec = PbrtImporter::import(std::move(parsed));
        expect(spec.instances()[0u].surface == spec.instances()[1u].surface);
    };

    "reject_out_of_range_inline_material"_test = []
    {
        auto parsed = PbrtParser::parse("tests/scenes/book_geometry.pbrt");
        parsed.shapes[0u].material.inline_index = 99u;
        auto source_located = false;
        try
        {
            (void)PbrtImporter::import(std::move(parsed));
        }
        catch (const std::runtime_error& error)
        {
            auto message  = std::string{error.what()};
            source_located = message.find("tests/scenes/book_geometry.pbrt") != std::string::npos &&
                             message.find("out-of-range inline material 99") != std::string::npos;
        }
        expect(source_located);
    };

    "reject_ambiguous_material_binding"_test = []
    {
        auto parsed = PbrtParser::parse("tests/scenes/book_geometry.pbrt");
        parsed.shapes[0u].material.named = "named";
        auto source_located = false;
        try
        {
            (void)PbrtImporter::import(std::move(parsed));
        }
        catch (const std::runtime_error& error)
        {
            auto message  = std::string{error.what()};
            source_located = message.find("tests/scenes/book_geometry.pbrt") != std::string::npos &&
                             message.find("both named and inline material bindings") != std::string::npos;
        }
        expect(source_located);
    };

    "reject_unsupported_inline_material"_test = []
    {
        auto parsed = PbrtParser::parse("tests/scenes/book_geometry.pbrt");
        parsed.materials[0u].type = MaterialDesc::Type::CoatedDiffuse;
        auto source_located = false;
        try
        {
            (void)PbrtImporter::import(std::move(parsed));
        }
        catch (const std::runtime_error& error)
        {
            auto message  = std::string{error.what()};
            source_located = message.find("tests/scenes/book_geometry.pbrt") != std::string::npos &&
                             message.find("Unsupported PBRT inline material") != std::string::npos;
        }
        expect(source_located);
    };

    "import_named_material_binding"_test = []
    {
        auto parsed = PbrtParser::parse("tests/scenes/book_geometry.pbrt");
        parsed.named_materials.emplace("named", parsed.materials[0u]);
        parsed.shapes[0u].material.named = "named";
        parsed.shapes[0u].material.inline_index.reset();
        auto spec = PbrtImporter::import(std::move(parsed));
        expect(spec.instances().size() == 3u);
        expect(spec.surfaces().size() == 4u);
    };

    "load_book_ply_geometry"_test = []
    {
        std::array<std::filesystem::path, 3u> paths{
            "scene/pbrt-book/geometry/mesh_00001.ply",
            "scene/pbrt-book/geometry/mesh_00002.ply",
            "scene/pbrt-book/geometry/mesh_00003.ply",
        };
        std::array<size_t, 3u> triangle_counts{20000u, 72u, 1174u};

        for (auto i = 0u; i < paths.size(); i++)
        {
            auto loader = MeshLoader::load(paths[i]);
            auto mesh   = loader->mesh();
            expect(!mesh.vertices.empty());
            expect(mesh.triangles.size() == triangle_counts[i]);
            expect((loader->properties() & Shape::property_flag_has_vertex_normal) != 0u);
            expect((loader->properties() & Shape::property_flag_has_vertex_uv) != 0u);
            for (auto triangle : mesh.triangles)
            {
                expect(triangle.i0 < mesh.vertices.size());
                expect(triangle.i1 < mesh.vertices.size());
                expect(triangle.i2 < mesh.vertices.size());
            }
        }
    };

    "reject_missing_ply_file"_test = []
    {
        auto parsed = PbrtParser::parse("tests/scenes/book_geometry.pbrt");
        parsed.shapes[0u].filename.emplace("../../scene/pbrt-book/geometry/missing.ply");
        auto source_located = false;
        try
        {
            (void)PbrtImporter::import(std::move(parsed));
        }
        catch (const std::runtime_error& error)
        {
            auto message  = std::string{error.what()};
            source_located = message.find("tests/scenes/book_geometry.pbrt") != std::string::npos &&
                             message.find("regular file") != std::string::npos;
        }
        expect(source_located);
    };
    return 0;
}();

} // namespace

int main(int argc, char* argv[])
{
    boost::ut::detail::cfg::parse_arg_with_fallback(argc, const_cast<const char**>(argv));
}
