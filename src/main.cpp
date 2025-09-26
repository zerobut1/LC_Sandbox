#include <luisa/dsl/syntax.h>
#include <luisa/runtime/context.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/stream.h>
#include <stb/stb_image_write.h>

using namespace luisa::compute;

int main(int argc, char** argv)
{
    Context context{argv[1]};
    Device device = context.create_device(argv[2]);
    Stream stream = device.create_stream();

    Kernel2D kernel = [](ImageVar<float> tex)
    {
        set_block_size(16, 16);
        UInt2 index = dispatch_id().xy();

        Float2 uv = (make_float2(index) + 0.5f) / make_float2(dispatch_size().xy());

        tex.write(index, make_float4(uv, 0.0f, 1.0f));
    };

    Kernel2D flip_x = [](ImageVar<float> tex, BindlessVar heap)
    {
        UInt2 index = dispatch_id().xy();

        Float2 uv = (make_float2(index) + 0.5f) / make_float2(dispatch_size().xy());
        uv.x      = 1.0f - uv.x;

        Float4 sample_result = heap.tex2d(0u).sample(uv);

        tex.write(index, make_float4(sample_result.xy(), 0.0f, 1.0f));
    };

    Shader shader        = device.compile(kernel);
    Shader shader_flip_x = device.compile(flip_x);

    constexpr uint32_t width  = 512;
    constexpr uint32_t height = 512;
    std::vector<std::byte> pixels(width * height * 4, std::byte{0});
    std::vector<std::byte> pixels_flip_x(width * height * 4, std::byte{0});

    Image<float> tex        = device.create_image<float>(PixelStorage::BYTE4, width, height);
    Image<float> tex_flip_x = device.create_image<float>(PixelStorage::BYTE4, width, height);

    BindlessArray heap = device.create_bindless_array();

    stream << heap.emplace_on_update(0, tex, Sampler::linear_linear_edge()).update();

    stream
        << shader(tex).dispatch(width, height)
        << shader_flip_x(tex_flip_x, heap).dispatch(width, height)
        << tex.copy_to(pixels.data())
        << tex_flip_x.copy_to(pixels_flip_x.data())
        << synchronize();

    stbi_write_png("test.png", width, height, 4, pixels.data(), 0);
    stbi_write_png("test_flip.png", width, height, 4, pixels_flip_x.data(), 0);

    return 0;
}