#include <luisa/luisa-compute.h>

#include <numeric>
#include <random>
#include <ranges>

using namespace luisa;
using namespace luisa::compute;

constexpr uint SEED       = 20120712u;
constexpr uint IMAGE_SIZE = 28u;
constexpr uint INPUT_SIZE = IMAGE_SIZE * IMAGE_SIZE;

[[nodiscard]] uint read_uint_be(std::ifstream& ifs) noexcept
{
    std::array<unsigned char, 4> buf{};
    ifs.read(reinterpret_cast<char*>(buf.data()), 4);
    LUISA_ASSERT(ifs, "Failed to read 4 bytes from file.");
    return (uint32_t(buf[0]) << 24) | (uint32_t(buf[1]) << 16) | (uint32_t(buf[2]) << 8) | uint32_t(buf[3]);
}

void read_mnist_images(string_view filename, vector<unsigned char>& images, uint& n_images) noexcept
{
    std::ifstream ifs(filename.data(), std::ios::binary);
    LUISA_ASSERT(ifs, "Cannot open file: {}", filename);

    [[maybe_unused]] auto temp = read_uint_be(ifs);
    n_images                   = read_uint_be(ifs);
    auto rows                  = read_uint_be(ifs);
    auto cols                  = read_uint_be(ifs);

    images.resize(n_images * rows * cols);
    ifs.read(reinterpret_cast<char*>(images.data()), images.size());
    LUISA_ASSERT(ifs, "Failed to read image data");
}

void read_mnist_labels(string_view filename, vector<unsigned char>& labels, uint& n_labels) noexcept
{
    std::ifstream ifs(filename.data(), std::ios::binary);
    LUISA_ASSERT(ifs, "Cannot open file: {}", filename);

    [[maybe_unused]] auto temp = read_uint_be(ifs);
    n_labels                   = read_uint_be(ifs);

    labels.resize(n_labels);
    ifs.read(reinterpret_cast<char*>(labels.data()), n_labels);
    LUISA_ASSERT(ifs, "Failed to read label data");
}

[[nodiscard]] float random_float() noexcept
{
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    static std::default_random_engine generator(SEED);
    return distribution(generator);
}

[[nodiscard]] float random_weight(float limit) noexcept
{
    return random_float() * 2.0f * limit - limit;
}

[[nodiscard]] Float ReLU(Float x) noexcept
{
    return max(0.0f, x);
}

[[nodiscard]] Float ReLU_deriv(Float x) noexcept
{
    return ite(x > 0.0f, 1.0f, 0.0f);
}

int main(int argc, char* argv[])
{
    Context context{argv[0]};

    if (argc <= 1)
    {
        LUISA_INFO("Usage: {} <backend>. <backend>: cuda, dx, vk", argv[0]);
        exit(1);
    }
    Device device = context.create_device(argv[1]);
    Stream stream = device.create_stream(StreamTag::COMPUTE);

    // ------------- 数据集 --------------
    // 读取数据集
    vector<unsigned char> origin_train_images;
    vector<unsigned char> origin_train_labels;
    vector<unsigned char> origin_test_images;
    vector<unsigned char> origin_test_labels;
    uint n_train_images, n_train_labels, n_test_images, n_test_labels;
    read_mnist_images("data/MNIST/train-images.idx3-ubyte", origin_train_images, n_train_images);
    read_mnist_labels("data/MNIST/train-labels.idx1-ubyte", origin_train_labels, n_train_labels);
    read_mnist_images("data/MNIST/t10k-images.idx3-ubyte", origin_test_images, n_test_images);
    read_mnist_labels("data/MNIST/t10k-labels.idx1-ubyte", origin_test_labels, n_test_labels);
    LUISA_ASSERT(n_train_images == n_train_labels, "Train Image and label count mismatch.");
    LUISA_ASSERT(n_test_images == n_test_labels, "Test Image and label count mismatch.");
    LUISA_INFO("MNIST dataset: {} training samples, {} testing samples.", n_train_labels, n_test_labels);

    const uint TRAIN_SIZE = n_train_labels;
    const uint TEST_SIZE  = n_test_labels;

    // 处理为需要的格式
    vector<float> host_train_images(origin_train_images.size());
    vector<uint> host_train_labels(n_train_labels);
    vector<float> host_test_images(origin_test_images.size());
    vector<uint> host_test_labels(n_test_labels);
    // 归一化处理
    std::transform(origin_train_images.begin(), origin_train_images.end(), host_train_images.begin(), [](unsigned char v)
    {
        return (static_cast<float>(v) / 255.0f - 0.1307f) / 0.3081f;
    });
    std::transform(origin_train_labels.begin(), origin_train_labels.end(), host_train_labels.begin(), [](unsigned char v)
    {
        return static_cast<uint>(v);
    });
    std::transform(origin_test_images.begin(), origin_test_images.end(), host_test_images.begin(), [](unsigned char v)
    {
        return (static_cast<float>(v) / 255.0f - 0.1307f) / 0.3081f;
    });
    std::transform(origin_test_labels.begin(), origin_test_labels.end(), host_test_labels.begin(), [](unsigned char v)
    {
        return static_cast<uint>(v);
    });

    // 上传到 GPU
    auto device_train_images = device.create_buffer<float>(host_train_images.size());
    auto device_train_labels = device.create_buffer<uint>(host_train_labels.size());
    auto device_test_images  = device.create_buffer<float>(host_test_images.size());
    auto device_test_labels  = device.create_buffer<uint>(host_test_labels.size());

    // 分块上传以避免 exceed CUDAHostBufferPool block size warning (188MB)
    {
        size_t total = host_train_images.size();
        size_t chunk = 1024u * 1024u * 8u; // 32MB chunks (floats) -> 8M elements
        for (size_t i = 0; i < total; i += chunk)
        {
            size_t n = (i + chunk < total) ? chunk : (total - i);
            stream << device_train_images.view(i, n).copy_from(host_train_images.data() + i);
        }
    }

    stream
        << device_train_labels.copy_from(host_train_labels.data())
        << device_test_images.copy_from(host_test_images.data())
        << device_test_labels.copy_from(host_test_labels.data())
        << synchronize();

    // ------------- 网络结构 --------------
    // 网络结构定义
    constexpr uint OUTPUT_SIZE = 10u;
    constexpr std::array<uint, 4> LAYER_DIMS{INPUT_SIZE, 128, 64, OUTPUT_SIZE};
    constexpr uint NUM_DIMS       = std::accumulate(LAYER_DIMS.begin(), LAYER_DIMS.end(), 0u);
    constexpr uint MAX_HIDDEN_DIM = *std::ranges::max_element(LAYER_DIMS | std::views::drop(1));
    constexpr auto LAYER_OFFSETS  = [&]() noexcept
    {
        std::array<uint, LAYER_DIMS.size()> offsets{};
        std::partial_sum(LAYER_DIMS.begin(), LAYER_DIMS.end() - 1, offsets.begin() + 1);
        return offsets;
    }();
    constexpr uint NUM_WEIGHT_LAYERS = static_cast<uint>(LAYER_DIMS.size() - 1);
    constexpr uint OUTPUT_START      = LAYER_OFFSETS.back();

    // 容器
    vector<Buffer<float>> weights;
    vector<Buffer<float>> biases;
    vector<Buffer<float>> momentums;
    vector<Buffer<int>> grad_weights;
    vector<Buffer<int>> grad_biases;

    // 初始化权重和偏置
    for (auto i = 0u; i < NUM_WEIGHT_LAYERS; i++)
    {
        auto in_dim  = LAYER_DIMS[i];
        auto out_dim = LAYER_DIMS[i + 1];

        vector<float> host_w(in_dim * out_dim);
        vector<float> host_m(in_dim * out_dim, 0.0f);
        vector<float> host_b(out_dim, 0.0f);

        auto limit = i == NUM_WEIGHT_LAYERS - 1
                         ? sqrt(6.0f / static_cast<float>(in_dim + out_dim))
                         : sqrt(2.0f / static_cast<float>(in_dim));

        for (auto& v : host_w)
        {
            v = random_weight(limit);
        }

        auto device_w = device.create_buffer<float>(host_w.size());
        auto device_b = device.create_buffer<float>(host_b.size());
        auto device_m = device.create_buffer<float>(host_m.size());

        stream
            << device_w.copy_from(host_w.data())
            << device_b.copy_from(host_b.data())
            << device_m.copy_from(host_m.data());

        weights.push_back(std::move(device_w));
        biases.push_back(std::move(device_b));
        momentums.push_back(std::move(device_m));
        grad_weights.push_back(device.create_buffer<int>(host_w.size()));
        grad_biases.push_back(device.create_buffer<int>(host_b.size()));
    }
    stream << synchronize();

    // ------------- Train Kernel --------------
    constexpr uint EPOCH_COUNT = 20u;
    constexpr uint BATCH_SIZE  = 256u;
    constexpr float BASE_LR    = 0.05f;
    // 使用缩放整数进行梯度累加，消除浮点原子加法的不确定性
    constexpr float GRAD_SCALE = static_cast<float>(1 << 20);

    auto loss    = device.create_buffer<float>(1u);
    auto correct = device.create_buffer<int>(1u);

    Kernel1D clear_int_buffer_kernel = [](BufferVar<int> buf) noexcept
    {
        buf.write(dispatch_id().x, 0);
    };

    Kernel1D clear_float_buffer_kernel = [](BufferVar<float> buf) noexcept
    {
        buf.write(dispatch_id().x, 0.0f);
    };

    auto clear_int_buffer_shader   = device.compile(clear_int_buffer_kernel);
    auto clear_float_buffer_shader = device.compile(clear_float_buffer_kernel);

    auto acts_buffer = device.create_buffer<float>(NUM_DIMS * BATCH_SIZE);

    Kernel2D transpose_kernel = [&](BufferVar<float> in, BufferVar<float> out)
    {
        // in: [batch, datas]
        // out: [datas, batch]
        UInt c = dispatch_id().x; // Data Index
        UInt r = dispatch_id().y; // Batch Index

        out.write(c * BATCH_SIZE + r, in.read(r * INPUT_SIZE + c));
    };
    auto transpose_shader = device.compile(transpose_kernel);

    Kernel2D gemm_kernel = [&](BufferVar<float> W, BufferVar<float> X, BufferVar<float> Bias, BufferVar<float> Y, UInt K, UInt activation)
    {
        set_block_size(16, 16, 1);
        UInt n = dispatch_id().x; // Batch Index
        UInt m = dispatch_id().y; // Output Feature Index

        Float sum = Bias.read(m);
        $for(k, K)
        {
            sum += W.read(m * K + k) * X.read(k * BATCH_SIZE + n);
        };

        $if(activation == 1)
        {
            sum = ReLU(sum);
        };
        Y.write(m * BATCH_SIZE + n, sum);
    };

    auto gemm_shader = device.compile(gemm_kernel);

    Kernel1D softmax_kernel = [&]()
    {
        UInt tid = dispatch_id().x;

        Float max_z = -1e30f;
        $for(i, OUTPUT_SIZE)
        {
            auto z = acts_buffer->read((OUTPUT_START + i) * BATCH_SIZE + tid);
            max_z  = max(max_z, z);
        };

        Float sum = 0.0f;
        $for(i, OUTPUT_SIZE)
        {
            auto z  = acts_buffer->read((OUTPUT_START + i) * BATCH_SIZE + tid);
            Float e = exp(z - max_z);
            sum += e;
            acts_buffer->write((OUTPUT_START + i) * BATCH_SIZE + tid, e);
        };

        $for(i, OUTPUT_SIZE)
        {
            auto idx = (OUTPUT_START + i) * BATCH_SIZE + tid;
            acts_buffer->write(idx, acts_buffer->read(idx) / sum);
        };
    };
    auto softmax_shader = device.compile(softmax_kernel);

    Kernel1D train_backward_kernel = [&](UInt batch_start, Float current_lr) noexcept
    {
        set_block_size(32u, 1u, 1u);
        auto tid     = dispatch_id().x;
        UInt data_id = batch_start + tid;

        // 1. 计算loss & accuracy
        UInt label = device_train_labels->read(data_id);

        ArrayFloat<NUM_DIMS> deltas;

        Float sample_loss = 0.0f;
        UInt pred         = 0u;
        Float best        = acts_buffer->read((OUTPUT_START + 0u) * BATCH_SIZE + tid);
        $for(c, OUTPUT_SIZE)
        {
            auto output_val = acts_buffer->read((OUTPUT_START + c) * BATCH_SIZE + tid);
            $if(output_val > best)
            {
                best = output_val;
                pred = c;
            };

            // 计算输出层delta
            // 激活函数 softmax
            // loss 交叉熵
            deltas[OUTPUT_START + c] = acts_buffer->read((OUTPUT_START + c) * BATCH_SIZE + tid) - ite(c == label, 1.0f, 0.0f);
        };
        auto target = acts_buffer->read((OUTPUT_START + label) * BATCH_SIZE + tid);
        loss->atomic(0u).fetch_add(-log(target));
        $if(pred == label) { correct->atomic(0u).fetch_add(1); };

        // 2. 反向传播
        for (auto i = NUM_WEIGHT_LAYERS - 1; i > 0u; i--)
        {
            auto in_start  = LAYER_OFFSETS[i];
            auto out_start = LAYER_OFFSETS[i + 1];
            auto in_dim    = LAYER_DIMS[i];
            auto out_dim   = LAYER_DIMS[i + 1];

            $for(j, in_dim)
            {
                Float sum_weighted_delta = 0.0f;
                $for(k, out_dim)
                {
                    sum_weighted_delta += deltas[out_start + k] * weights[i]->read(k * in_dim + j);
                };

                Float d_activation   = i == 0 ? 1.0f : ReLU_deriv(acts_buffer->read((in_start + j) * BATCH_SIZE + tid));
                deltas[in_start + j] = sum_weighted_delta * d_activation;
            };
        }

        // 3. 累计 batch 梯度
        for (auto i = 0u; i < NUM_WEIGHT_LAYERS; i++)
        {
            auto in_start  = LAYER_OFFSETS[i];
            auto out_start = LAYER_OFFSETS[i + 1];
            auto in_dim    = LAYER_DIMS[i];
            auto out_dim   = LAYER_DIMS[i + 1];

            // 重排循环：每个输入激活只读一次，更新所有输出的权重梯度
            $for(j, in_dim)
            {
                auto input_val = acts_buffer->read((in_start + j) * BATCH_SIZE + tid);
                $for(c, out_dim)
                {
                    auto d = deltas[out_start + c];
                    grad_weights[i]->atomic(c * in_dim + j).fetch_add(cast<int>(input_val * d * GRAD_SCALE));
                };
            };
            // bias 梯度仍然按输出维度累计一次即可
            $for(c, out_dim)
            {
                auto d = deltas[out_start + c];
                grad_biases[i]->atomic(c).fetch_add(cast<int>(d * GRAD_SCALE));
            };
        }
    };

    auto train_backward_shader = device.compile(train_backward_kernel);

    Kernel1D update_kernel = [&](Float current_lr, UInt batch_size)
    {
        auto tid    = dispatch_id().x;
        auto size   = dispatch_size().x;
        Float inv_n = 1.0f / cast<float>(batch_size);

        for (auto i = 0u; i < NUM_WEIGHT_LAYERS; i++)
        {
            auto in_dim  = LAYER_DIMS[i];
            auto out_dim = LAYER_DIMS[i + 1];
            auto w_num   = in_dim * out_dim;
            auto b_num   = out_dim;

            // 并行更新权重 (Grid-Stride Loop)
            UInt k = tid;
            $while(k < w_num)
            {
                Float g = cast<float>(grad_weights[i]->read(k)) / GRAD_SCALE * inv_n;
                auto m  = momentums[i]->read(k) * 0.9f + g;
                auto w  = weights[i]->read(k);

                w = w - current_lr * m - 5e-4f * w * current_lr;
                weights[i]->write(k, w);
                momentums[i]->write(k, m);
                // 重置梯度，准备下一轮
                grad_weights[i]->write(k, 0);
                k += size;
            };

            // 并行更新偏置
            k = tid;
            $while(k < b_num)
            {
                Float g = cast<float>(grad_biases[i]->read(k)) / GRAD_SCALE * inv_n;
                biases[i]->write(k, biases[i]->read(k) - current_lr * g);
                grad_biases[i]->write(k, 0);
                k += size;
            };
        }
    };

    auto update_shader = device.compile(update_kernel);

    // 初始清空梯度
    for (auto layer_idx = 0u; layer_idx < NUM_WEIGHT_LAYERS; layer_idx++)
    {
        stream
            << clear_int_buffer_shader(grad_weights[layer_idx]).dispatch(grad_weights[layer_idx].size())
            << clear_int_buffer_shader(grad_biases[layer_idx]).dispatch(grad_biases[layer_idx].size());
    }

    // 训练
    LUISA_INFO("Start Traning!");
    Clock train_clock;
    for (auto epoch = 0u; epoch < EPOCH_COUNT; epoch++)
    {
        Clock epoch_clock;

        // Cosine Annealing Learning Rate Schedule
        // LR 从 BASE_LR 随 epoch 按照余弦曲线逐渐衰减到 0
        float current_lr = BASE_LR * 0.5f * (1.0f + cos(static_cast<float>(epoch) * pi / static_cast<float>(EPOCH_COUNT)));

        stream
            << clear_float_buffer_shader(loss).dispatch(1u)
            << clear_int_buffer_shader(correct).dispatch(1u);
        for (auto batch_start = 0u; batch_start < TRAIN_SIZE; batch_start += BATCH_SIZE)
        {
            // 不能整除的时候，调整最后一个batch dispatch的大小
            auto dispatch_size = min(BATCH_SIZE, TRAIN_SIZE - batch_start);

            stream << transpose_shader(device_train_images.view(batch_start * INPUT_SIZE, dispatch_size * INPUT_SIZE),
                                       acts_buffer.view(0, INPUT_SIZE * BATCH_SIZE))
                          .dispatch(INPUT_SIZE, dispatch_size);

            for (auto i = 0u; i < NUM_WEIGHT_LAYERS; i++)
            {
                auto in_start   = LAYER_OFFSETS[i];
                auto out_start  = LAYER_OFFSETS[i + 1];
                auto in_dim     = LAYER_DIMS[i];
                auto out_dim    = LAYER_DIMS[i + 1];
                auto activation = (i == NUM_WEIGHT_LAYERS - 1) ? 0u : 1u;

                stream << gemm_shader(weights[i],
                                      acts_buffer.view(in_start * BATCH_SIZE, in_dim * BATCH_SIZE),
                                      biases[i],
                                      acts_buffer.view(out_start * BATCH_SIZE, out_dim * BATCH_SIZE),
                                      in_dim,
                                      activation)
                              .dispatch(dispatch_size, out_dim);
            }

            stream
                << softmax_shader().dispatch(dispatch_size)
                << train_backward_shader(batch_start, current_lr).dispatch(dispatch_size)
                << update_shader(current_lr, dispatch_size).dispatch(NUM_DIMS);
        }

        auto current_loss    = 0.0f;
        auto current_correct = 0;
        stream
            << loss.copy_to(&current_loss)
            << correct.copy_to(&current_correct)
            << synchronize();

        auto current_acc = static_cast<float>(current_correct) / static_cast<float>(TRAIN_SIZE);
        LUISA_INFO("Epoch {}: Loss = {:.8f}, Acc = {:.2f}%, time = {:.2f}s",
                   epoch + 1,
                   current_loss / static_cast<float>(TRAIN_SIZE),
                   100.0f * current_acc,
                   epoch_clock.toc() * 1e-3);
    }
    LUISA_INFO("Training {} epoches completed in {:.2f} seconds.", EPOCH_COUNT, train_clock.toc() * 1e-3);

    stream << synchronize();

    // 测试
    auto test_loss    = device.create_buffer<float>(1u);
    auto test_correct = device.create_buffer<int>(1u);

    Kernel1D test_kernel = [&]() noexcept
    {
        auto data_id = dispatch_id().x;

        $if(data_id < TEST_SIZE)
        {
            ArrayFloat<NUM_DIMS> acts;

            // Input
            $for(k, INPUT_SIZE)
            {
                acts[k] = device_test_images->read(data_id * INPUT_SIZE + k);
            };
            UInt label = clamp(device_test_labels->read(data_id), 0u, OUTPUT_SIZE - 1u);

            // 前向传播
            for (auto i = 0; i < NUM_WEIGHT_LAYERS; i++)
            {
                auto in_start  = LAYER_OFFSETS[i];
                auto out_start = LAYER_OFFSETS[i + 1];
                auto in_dim    = LAYER_DIMS[i];
                auto out_dim   = LAYER_DIMS[i + 1];

                $for(j, out_dim)
                {
                    Float z = biases[i]->read(j);
                    $for(k, in_dim)
                    {
                        auto act_in = acts[in_start + k];
                        auto w      = weights[i]->read(j * in_dim + k);
                        z += act_in * w;
                    };
                    acts[out_start + j] = i == NUM_WEIGHT_LAYERS - 1 ? z : ReLU(z);
                };
            }

            // 输出节点用softmax作为激活函数
            auto z_max = acts[OUTPUT_START + 0u];
            $for(c, OUTPUT_SIZE)
            {
                z_max = max(z_max, acts[OUTPUT_START + c]);
            };

            auto sum_exp = def(0.0f);
            $for(c, OUTPUT_SIZE)
            {
                auto z     = acts[OUTPUT_START + c];
                auto exp_z = exp(z - z_max);

                acts[OUTPUT_START + c] = exp_z;
                sum_exp += exp_z;
            };

            $for(c, OUTPUT_SIZE)
            {
                acts[OUTPUT_START + c] = acts[OUTPUT_START + c] / sum_exp;
            };

            // loss & accuracy
            UInt pred  = 0u;
            Float best = acts[OUTPUT_START + 0u];
            $for(c, OUTPUT_SIZE)
            {
                auto val = acts[OUTPUT_START + c];

                $if(val > best)
                {
                    best = val;
                    pred = c;
                };
            };
            test_loss->atomic(0u).fetch_add(-log(acts[OUTPUT_START + label]));
            $if(pred == label) { test_correct->atomic(0u).fetch_add(1); };
        };
    };
    auto test_shader = device.compile(test_kernel);

    auto current_test_loss    = 0.0f;
    auto current_test_correct = 0;

    LUISA_INFO("Start Testing!");
    Clock test_clock;
    stream
        << clear_float_buffer_shader(test_loss).dispatch(1u)
        << clear_int_buffer_shader(test_correct).dispatch(1u)
        << test_shader().dispatch(TEST_SIZE)
        << test_loss.copy_to(&current_test_loss)
        << test_correct.copy_to(&current_test_correct)
        << synchronize();

    current_test_loss /= static_cast<float>(TEST_SIZE);
    LUISA_INFO("Testing completed in {:.2f} seconds", test_clock.toc() * 1e-3);
    LUISA_INFO("Test Average Loss = {}, Accuracy = {:.2f}% ({}/{})",
               current_test_loss,
               100.0f * static_cast<float>(current_test_correct) / static_cast<float>(TEST_SIZE),
               current_test_correct,
               TEST_SIZE);

    return 0;
}