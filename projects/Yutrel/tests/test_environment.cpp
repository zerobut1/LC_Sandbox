#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

#include <luisa/luisa-compute.h>

#include "base/renderer.h"
#include "base/scene.h"
#include "environments/pbrt_equal_area.h"
#include "pbrt/pbrt_importer.h"
#include "pbrt/pbrt_parser.h"
#include "utils/image_io.h"
#include "utils/sampling.h"

using namespace luisa;
using namespace luisa::compute;
using namespace Yutrel;

namespace
{

[[nodiscard]] uint32_t byte_swap_32(uint32_t v) noexcept
{
    return ((v & 0x000000ffu) << 24u) |
           ((v & 0x0000ff00u) << 8u) |
           ((v & 0x00ff0000u) >> 8u) |
           ((v & 0xff000000u) >> 24u);
}

void write_pfm(const std::filesystem::path& path, const char* magic,
               uint width, uint height, float scale,
               luisa::span<const float> values)
{
    std::ofstream stream{path, std::ios::binary};
    stream << magic << '\n'
           << width << ' ' << height << '\n'
           << scale << '\n';
    auto file_little_endian           = scale < 0.0f;
    constexpr auto host_little_endian = std::endian::native == std::endian::little;
    for (auto value : values)
    {
        uint32_t bits{};
        std::memcpy(&bits, &value, sizeof(bits));
        if (file_little_endian != host_little_endian)
        {
            bits = byte_swap_32(bits);
        }
        stream.write(reinterpret_cast<const char*>(&bits), sizeof(bits));
    }
}

[[nodiscard]] bool nearly_equal(float a, float b, float epsilon = 2e-4f) noexcept
{
    return std::abs(a - b) <= epsilon;
}

[[nodiscard]] bool test_alias_tables() noexcept
{
    std::array zeros{0.0f, 0.0f, 0.0f, 0.0f};
    auto [zero_aliases, zero_pdf] = create_alias_table(zeros);
    for (auto i = 0u; i < zeros.size(); i++)
    {
        if (zero_aliases[i].prob != 1.0f || zero_aliases[i].alias != i ||
            !nearly_equal(zero_pdf[i], 0.25f))
        {
            return false;
        }
    }
    std::array values{1.0f, 2.0f, 3.0f, 4.0f};
    auto [aliases, pdf] = create_alias_table(values);
    auto sum            = 0.0f;
    for (auto p : pdf)
    {
        if (!std::isfinite(p) || p < 0.0f)
        {
            return false;
        }
        sum += p;
    }
    return aliases.size() == values.size() && nearly_equal(sum, 1.0f);
}

[[nodiscard]] bool test_pfm_loading()
{
    auto directory = std::filesystem::temp_directory_path();
    auto rgb_path  = directory / "yutrel_environment_rgb.pfm";
    auto gray_path = directory / "yutrel_environment_gray.pfm";

    // PFM scanlines are stored bottom-up. The file values below contain the
    // bottom row first and the top row second.
    std::array rgb_values{
        10.0f,
        11.0f,
        12.0f,
        20.0f,
        21.0f,
        22.0f,
        1.0f,
        2.0f,
        3.0f,
        4.0f,
        5.0f,
        6.0f,
    };
    write_pfm(rgb_path, "PF", 2u, 2u, -2.0f, rgb_values);
    auto rgb        = LoadedImage::load(rgb_path);
    auto rgb_pixels = static_cast<const float*>(rgb.pixels());
    auto rgb_valid  = rgb.size().x == 2u && rgb.size().y == 2u &&
                      rgb.pixel_storage() == PixelStorage::FLOAT4 &&
                      nearly_equal(rgb_pixels[0u], 2.0f) &&
                      nearly_equal(rgb_pixels[1u], 4.0f) &&
                      nearly_equal(rgb_pixels[2u], 6.0f) &&
                      nearly_equal(rgb_pixels[3u], 1.0f) &&
                      nearly_equal(rgb_pixels[8u], 20.0f);

    std::array gray_values{8.0f, 4.0f};
    write_pfm(gray_path, "Pf", 1u, 2u, 0.5f, gray_values);
    auto gray        = LoadedImage::load(gray_path);
    auto gray_pixels = static_cast<const float*>(gray.pixels());
    auto gray_valid  = gray.size().x == 1u && gray.size().y == 2u &&
                       gray.pixel_storage() == PixelStorage::FLOAT1 &&
                       nearly_equal(gray_pixels[0u], 2.0f) &&
                       nearly_equal(gray_pixels[1u], 4.0f);

    std::error_code error;
    std::filesystem::remove(rgb_path, error);
    std::filesystem::remove(gray_path, error);
    return rgb_valid && gray_valid;
}

[[nodiscard]] bool test_equal_area_mapping(Device& device, Stream& stream)
{
    std::array directions{
        make_float3(1.0f, 0.0f, 0.0f),
        make_float3(-1.0f, 0.0f, 0.0f),
        make_float3(0.0f, 1.0f, 0.0f),
        make_float3(0.0f, -1.0f, 0.0f),
        make_float3(0.0f, 0.0f, 1.0f),
        make_float3(0.0f, 0.0f, -1.0f),
    };
    auto direction_input      = device.create_buffer<float3>(directions.size());
    auto direction_output     = device.create_buffer<float4>(directions.size());
    Kernel1D direction_kernel = [](BufferFloat3 input, BufferFloat4 output) noexcept
    {
        auto i         = dispatch_id().x;
        auto direction = input.read(i);
        auto uv        = equal_area_sphere_to_square(direction);
        auto recovered = equal_area_square_to_sphere(uv);
        output.write(i, make_float4(recovered, length(recovered)));
    };
    auto test_directions = device.compile(direction_kernel);
    std::array<float4, directions.size()> recovered_directions{};
    stream << direction_input.copy_from(directions.data())
           << test_directions(direction_input, direction_output).dispatch(directions.size())
           << direction_output.copy_to(recovered_directions.data())
           << synchronize();
    for (auto i = 0u; i < directions.size(); i++)
    {
        auto recovered = recovered_directions[i].xyz();
        if (!nearly_equal(recovered_directions[i].w, 1.0f, 1e-3f) ||
            dot(recovered, directions[i]) < 0.999f)
        {
            return false;
        }
    }

    std::array<float2, 32u> uvs{};
    for (auto i = 0u; i < uvs.size(); i++)
    {
        // Stay away from the measure-zero square boundary where equivalent
        // seam representations are expected.
        uvs[i] = make_float2(
            0.02f + 0.96f * std::fmod(i * 0.61803398875f, 1.0f),
            0.02f + 0.96f * std::fmod(i * 0.41421356237f, 1.0f));
    }
    auto uv_input      = device.create_buffer<float2>(uvs.size());
    auto uv_output     = device.create_buffer<float2>(uvs.size());
    Kernel1D uv_kernel = [](BufferFloat2 input, BufferFloat2 output) noexcept
    {
        auto i  = dispatch_id().x;
        auto uv = input.read(i);
        output.write(i, equal_area_sphere_to_square(equal_area_square_to_sphere(uv)));
    };
    auto test_uvs = device.compile(uv_kernel);
    std::array<float2, uvs.size()> recovered_uvs{};
    stream << uv_input.copy_from(uvs.data())
           << test_uvs(uv_input, uv_output).dispatch(uvs.size())
           << uv_output.copy_to(recovered_uvs.data())
           << synchronize();
    for (auto i = 0u; i < uvs.size(); i++)
    {
        if (!nearly_equal(uvs[i].x, recovered_uvs[i].x, 2e-3f) ||
            !nearly_equal(uvs[i].y, recovered_uvs[i].y, 2e-3f))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] luisa::unique_ptr<Scene> load_infinite_diffuse_scene()
{
    auto parsed      = PbrtParser::parse("tests/scenes/infinite_diffuse.pbrt");
    auto spec        = PbrtImporter::import(std::move(parsed));
    auto scene       = Scene::create(spec);
    auto environment = dynamic_cast<const PBRTEqualAreaEnvironment*>(scene->environment());
    if (environment == nullptr || environment->emission()->resolution().x != 1024u ||
        environment->emission()->resolution().y != 1024u ||
        environment->emission()->channels() < 3u)
    {
        return nullptr;
    }
    return scene;
}

[[nodiscard]] bool test_infinite_environment(
    Device& device, Stream& stream, const Scene& scene)
{
    auto renderer = Renderer::create(device, stream, scene);
    if (renderer->environment() == nullptr || !renderer->lights().empty())
    {
        return false;
    }

    auto output     = device.create_buffer<float4>(1u);
    Kernel1D kernel = [&renderer](BufferFloat4 result) noexcept
    {
        auto swl       = renderer->spectrum()->sample(0.5f);
        auto sample    = renderer->environment()->sample(swl, 0.0f, make_float2(0.37f, 0.73f));
        auto evaluated = renderer->environment()->evaluate(sample.wi, swl, 0.0f);
        result.write(0u, make_float4(sample.eval.pdf, evaluated.pdf, length(sample.wi), sample.eval.L[0u]));
    };
    auto shader = device.compile(kernel);
    std::array<float4, 1u> result{};
    stream << shader(output).dispatch(1u)
           << output.copy_to(result.data())
           << synchronize();
    auto value = result.front();
    return std::isfinite(value.x) && value.x >= 0.0f &&
           std::isfinite(value.y) && value.y >= 0.0f &&
           nearly_equal(value.x, value.y, 1e-5f) &&
           nearly_equal(value.z, 1.0f, 1e-3f) &&
           std::isfinite(value.w) && value.w >= 0.0f;
}

} // namespace

int main(int argc, char* argv[])
{
    if (!test_alias_tables() || !test_pfm_loading())
    {
        return 2;
    }
    auto scene = load_infinite_diffuse_scene();
    if (scene == nullptr)
    {
        return 3;
    }
    if (argc < 2)
    {
        return 0;
    }

    Context context{argv[0]};
    auto device = context.create_device(argv[1]);
    auto stream = device.create_stream();
    if (!test_equal_area_mapping(device, stream))
    {
        return 4;
    }
    if (!test_infinite_environment(device, stream, *scene))
    {
        return 5;
    }
    return 0;
}
