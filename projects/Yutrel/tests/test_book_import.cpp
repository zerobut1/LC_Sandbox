#include "ut/ut.hpp"

#include "base/shape.h"
#include "pbrt/pbrt_importer.h"
#include "pbrt/pbrt_parser.h"
#include "shapes/mesh.h"
#include "shapes/sphere.h"
#include "textures/image.h"
#include "textures/constant.h"
#include "textures/scale.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
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
    "generate_sphere_geometry"_test = []
    {
        Sphere sphere_level_0{2.0f, 0u};
        auto coarse = sphere_level_0.mesh();
        expect(coarse.vertices.size() == 12u);
        expect(coarse.triangles.size() == 20u);

        Sphere sphere_level_4{2.0f};
        auto mesh = sphere_level_4.mesh();
        expect(mesh.vertices.size() == 2562u);
        expect(mesh.triangles.size() == 5120u);
        expect((sphere_level_4.vertex_properties() & Shape::property_flag_has_vertex_normal) != 0u);
        expect((sphere_level_4.vertex_properties() & Shape::property_flag_has_vertex_uv) != 0u);

        for (auto vertex : mesh.vertices)
        {
            auto p = vertex.position();
            auto n = vertex.normal();
            expect(is_near(std::sqrt(luisa::dot(p, p)), 2.0f));
            expect(is_near(std::sqrt(luisa::dot(n, n)), 1.0f));
            expect(luisa::dot(p, n) > 0.0f);
            expect(std::isfinite(vertex.u) && std::isfinite(vertex.v));
        }
        for (auto triangle : mesh.triangles)
        {
            expect(triangle.i0 < mesh.vertices.size());
            expect(triangle.i1 < mesh.vertices.size());
            expect(triangle.i2 < mesh.vertices.size());
            auto p0 = mesh.vertices[triangle.i0].position();
            auto p1 = mesh.vertices[triangle.i1].position();
            auto p2 = mesh.vertices[triangle.i2].position();
            expect(luisa::dot(luisa::cross(p1 - p0, p2 - p0), p0) > 0.0f);
        }
    };

    "validate_sphere_spec"_test = []
    {
        expect(!SphereShapeSpec{1.0f}.validate().has_value());
        expect(SphereShapeSpec{0.0f}.validate().has_value());
        expect(SphereShapeSpec{-1.0f}.validate().has_value());
        expect(SphereShapeSpec{std::numeric_limits<float>::infinity()}.validate().has_value());
        expect(SphereShapeSpec{std::numeric_limits<float>::quiet_NaN()}.validate().has_value());
        expect(SphereShapeSpec{1.0f, Sphere::max_subdivision + 1u}.validate().has_value());
    };

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

    "import_book_v2_spheres"_test = []
    {
        auto parsed = PbrtParser::parse("scene/pbrt-book/book-v2.pbrt");
        auto spec   = PbrtImporter::import(std::move(parsed));
        expect(spec.shapes().size() == 5u);
        expect(spec.instances().size() == 5u);
        expect(spec.lights().size() == 2u);

        auto instances = spec.instances();
        auto sphere_0  = dynamic_cast<const SphereShapeSpec*>(&spec.shapes().spec(instances[0u].shape));
        auto sphere_1  = dynamic_cast<const SphereShapeSpec*>(&spec.shapes().spec(instances[1u].shape));
        expect(sphere_0 != nullptr);
        expect(sphere_1 != nullptr);
        if (sphere_0 != nullptr && sphere_1 != nullptr)
        {
            expect(is_near(sphere_0->radius(), 7.5f));
            expect(is_near(sphere_1->radius(), 7.5f));
            expect(sphere_0->subdivision() == Sphere::default_subdivision);
            expect(sphere_1->subdivision() == Sphere::default_subdivision);
        }
        expect(instances[0u].surface == instances[1u].surface);
        expect(instances[0u].light.has_value());
        expect(instances[1u].light.has_value());
        expect(instances[0u].light != instances[1u].light);
        expect(is_near(instances[0u].transform[3u].x, 34.92f));
        expect(is_near(instances[0u].transform[3u].y, 55.92f));
        expect(is_near(instances[0u].transform[3u].z, -15.351f));

        auto image_texture_count = 0u;
        std::array<std::filesystem::path, 3u> expected_texture_paths{
            std::filesystem::absolute("scene/pbrt-book/texture/book_pbrt.png"),
            std::filesystem::absolute("scene/pbrt-book/texture/book_pages.png"),
            std::filesystem::absolute("scene/pbrt-book/texture/uneven_bump.png"),
        };
        auto found_bump_constant = false;
        auto found_bump_scale = false;
        spec.textures().visit_entries([&](TextureRef, const SpecMeta& meta, const TextureSpec* texture)
        {
            auto image = dynamic_cast<const ImageTextureSpec*>(texture);
            if (image != nullptr)
            {
                expect(image_texture_count < expected_texture_paths.size());
                if (image_texture_count < expected_texture_paths.size())
                {
                    expect(image->path().lexically_normal() == expected_texture_paths[image_texture_count].lexically_normal());
                }
                auto expected_uv_scale = meta.name == "uneven_bump_raw" ? 1.5f : 1.0f;
                expect(is_near(image->uv_scale().x, expected_uv_scale));
                expect(is_near(image->uv_scale().y, expected_uv_scale));
                expect(is_near(image->uv_offset().x, 0.0f));
                expect(is_near(image->uv_offset().y, 0.0f));
                if (meta.name == "uneven_bump_raw")
                {
                    expect(image->encoding() == Texture::Encoding::LINEAR);
                }
                image_texture_count++;
                return;
            }
            if (meta.name == "uneven_bump_scale")
            {
                auto constant = dynamic_cast<const ConstantTextureSpec*>(texture);
                expect(constant != nullptr);
                if (constant != nullptr)
                {
                    expect(is_near(constant->value().x, 0.0002f));
                }
                found_bump_constant = true;
            }
            if (meta.name == "uneven_bump")
            {
                auto scale = dynamic_cast<const ScaleTextureSpec*>(texture);
                expect(scale != nullptr);
                if (scale != nullptr)
                {
                    expect(is_near(scale->scale().x, 0.0002f));
                    expect(is_near(scale->offset().x, 0.0f));
                }
                found_bump_scale = true;
            }
        });
        expect(image_texture_count == expected_texture_paths.size());
        expect(found_bump_constant);
        expect(found_bump_scale);
        expect(instances[3u].surface != instances[4u].surface);
    };

    "detect_image_texture_channels"_test = []
    {
        ImageTexture pages{
            "scene/pbrt-book/texture/book_pages.png",
            TextureSampler::linear_point_repeat(),
            Texture::Encoding::SRGB};
        ImageTexture cover{
            "scene/pbrt-book/texture/book_pbrt.png",
            TextureSampler::linear_point_repeat(),
            Texture::Encoding::SRGB};
        expect(pages.channels() == 1u);
        expect(cover.channels() == 4u);
    };

    "fold_static_scale_texture"_test = []
    {
        ConstantTexture base{make_float4(2.0f, 3.0f, 4.0f, 1.0f)};
        ScaleTexture scaled{&base, make_float4(0.5f), make_float4(1.0f)};
        auto value = scaled.evaluate_static();
        expect(value.has_value());
        if (value)
        {
            expect(is_near(value->x, 2.0f));
            expect(is_near(value->y, 2.5f));
            expect(is_near(value->z, 3.0f));
            expect(is_near(value->w, 1.5f));
        }
        expect(scaled.channels() == base.channels());
    };

    "reject_unknown_reflectance_texture"_test = []
    {
        auto parsed = PbrtParser::parse("scene/pbrt-book/book-v2.pbrt");
        parsed.materials[1u].reflectance_texture.emplace("missing_texture");
        auto rejected = false;
        try
        {
            (void)PbrtImporter::import(std::move(parsed));
        }
        catch (const std::runtime_error& error)
        {
            auto message = std::string{error.what()};
            rejected = message.find("book-v2.pbrt") != std::string::npos &&
                       message.find("missing_texture") != std::string::npos;
        }
        expect(rejected);
    };

    "reject_dynamic_scale_texture"_test = []
    {
        auto parsed = PbrtParser::parse("scene/pbrt-book/book-v2.pbrt");
        parsed.textures[4u].scale = "book_cover";
        auto rejected = false;
        try
        {
            (void)PbrtImporter::import(std::move(parsed));
        }
        catch (const std::runtime_error& error)
        {
            auto message = std::string{error.what()};
            rejected = message.find("book-v2.pbrt") != std::string::npos &&
                       message.find("float constant") != std::string::npos &&
                       message.find("dynamic multiplication") != std::string::npos;
        }
        expect(rejected);
    };

    "reject_unknown_scale_texture"_test = []
    {
        auto parsed = PbrtParser::parse("scene/pbrt-book/book-v2.pbrt");
        parsed.textures[4u].scale = "missing_scale";
        auto rejected = false;
        try
        {
            (void)PbrtImporter::import(std::move(parsed));
        }
        catch (const std::runtime_error& error)
        {
            auto message = std::string{error.what()};
            rejected = message.find("book-v2.pbrt") != std::string::npos &&
                       message.find("missing_scale") != std::string::npos &&
                       message.find("unknown scale texture") != std::string::npos;
        }
        expect(rejected);
    };

    "reject_missing_image_texture_file"_test = []
    {
        auto parsed = PbrtParser::parse("scene/pbrt-book/book-v2.pbrt");
        parsed.textures[0u].filename = "texture/missing.png";
        auto rejected = false;
        try
        {
            (void)PbrtImporter::import(std::move(parsed));
        }
        catch (const std::runtime_error& error)
        {
            auto message = std::string{error.what()};
            rejected = message.find("book-v2.pbrt") != std::string::npos &&
                       message.find("regular file") != std::string::npos;
        }
        expect(rejected);
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
