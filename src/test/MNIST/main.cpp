#include <cstdint>
#include <fstream>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/window.h>
#include <luisa/luisa-compute.h>
#include <numeric>
#include <random>
#include <stb/stb_image_write.h>

using namespace luisa;
using namespace luisa::compute;

const uint SEED = 20120712u;

float random_float()
{
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    static std::default_random_engine generator(SEED);
    return distribution(generator);
}

float random_weight(float limit)
{
    return random_float() * 2.0f * limit - limit;
}

Float sigmoid(Float x)
{
    return 1.0f / (1.0f + exp(-x));
}

Float sigmoid_deriv(Float x)
{
    Float fx = sigmoid(x);
    return fx * (1 - fx);
}

Float ReLU(Float x)
{
    return max(0.0f, x);
}

Float ReLU_deriv(Float x)
{
    return ite(x > 0.0f, 1.0f, 0.0f);
}

const uint IMAGE_SIZE = 28u;

inline int32_t read_int32_be(std::ifstream& ifs)
{
    int32_t val = 0;
    ifs.read(reinterpret_cast<char*>(&val), 4);
    val = ((val & 0xff) << 24) | ((val & 0xff00) << 8) | ((val & 0xff0000) >> 8) | ((val >> 24) & 0xff);
    return val;
}

void read_mnist_images(string_view filename, vector<unsigned char>& images, uint& n_images)
{
    std::ifstream ifs(filename.data(), std::ios::binary);
    if (!ifs)
        LUISA_ERROR("Cannot open file: {}", filename);

    uint temp, rows, cols;

    temp     = read_int32_be(ifs);
    n_images = read_int32_be(ifs);
    rows     = read_int32_be(ifs);
    cols     = read_int32_be(ifs);

    images.resize(n_images * rows * cols);
    ifs.read(reinterpret_cast<char*>(images.data()), images.size());
    if (!ifs)
        LUISA_ERROR("Failed to read image data");
}

void read_mnist_labels(string_view filename, vector<unsigned char>& labels, uint& n_labels)
{
    std::ifstream ifs(filename.data(), std::ios::binary);
    if (!ifs)
        LUISA_ERROR("Cannot open file: {}", filename);

    uint temp;
    temp     = read_int32_be(ifs);
    n_labels = read_int32_be(ifs);

    labels.resize(n_labels);
    ifs.read(reinterpret_cast<char*>(labels.data()), n_labels);
    if (!ifs)
        LUISA_ERROR("Failed to read label data");
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

    // 处理为需要的格式
    std::vector<float> host_train_images(origin_train_images.size());
    std::vector<uint> host_train_labels(n_train_labels);
    std::vector<float> host_test_images(origin_test_images.size());
    std::vector<uint> host_test_labels(n_test_labels);
    std::transform(origin_train_images.begin(), origin_train_images.end(), host_train_images.begin(), [](unsigned char v)
    {
        return static_cast<float>(v) / 255.0f;
    });
    std::transform(origin_train_labels.begin(), origin_train_labels.end(), host_train_labels.begin(), [](unsigned char v)
    {
        return static_cast<uint>(v);
    });
    std::transform(origin_test_images.begin(), origin_test_images.end(), host_test_images.begin(), [](unsigned char v)
    {
        return static_cast<float>(v) / 255.0f;
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

    const uint TRAIN_SIZE = n_train_labels;
    const uint TEST_SIZE  = n_test_labels;

    // ------------- 网络结构 --------------
    // 网络结构定义
    constexpr uint MAX_TOTAL_NODES = 4096;
    constexpr uint INPUT_SIZE      = IMAGE_SIZE * IMAGE_SIZE;
    constexpr uint OUTPUT_SIZE     = 10;
    constexpr std::array<uint, 4> LAYERS{INPUT_SIZE, 256, 128, OUTPUT_SIZE};
    constexpr uint NUM_NODES = std::accumulate(LAYERS.begin(), LAYERS.end(), 0u);

    std::vector<uint> layer_offsets(LAYERS.size());
    auto node_count = 0u;
    for (auto i = 0u; i < LAYERS.size(); i++)
    {
        layer_offsets[i] = node_count;
        node_count += LAYERS[i];
    }
    LUISA_ASSERT(node_count <= MAX_TOTAL_NODES, "Too many nodes in the network.");
    const uint NUM_WEIGHT_LAYERS = static_cast<uint>(LAYERS.size() - 1);
    const uint OUTPUT_START      = layer_offsets.back();

    // 容器
    vector<Buffer<float>> weights;
    vector<Buffer<float>> biases;
    vector<Buffer<float>> momentums;
    vector<Buffer<int>> grad_weights;
    vector<Buffer<int>> grad_biases;

    // 初始化权重和偏置
    for (auto i = 0u; i < NUM_WEIGHT_LAYERS; i++)
    {
        auto in_size  = LAYERS[i];
        auto out_size = LAYERS[i + 1];

        std::vector<float> host_w(in_size * out_size);
        std::vector<float> host_m(in_size * out_size, 0.0f);
        std::vector<float> host_b(out_size, 0.0f);

        auto limit = i < NUM_WEIGHT_LAYERS - 1
                         ? std::sqrt(6.0f / static_cast<float>(in_size))
                         : std::sqrt(6.0f / static_cast<float>(in_size + out_size));

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
    constexpr uint BATCH_SIZE  = 64u;
    constexpr float LR         = 0.01f;
    // 使用缩放整数进行梯度累加，消除浮点原子加法的不确定性
    constexpr float GRAD_SCALE = 1048576.0f;

    auto loss    = device.create_buffer<float>(1);
    auto correct = device.create_buffer<uint>(1);

    Kernel1D train_kernel = [&](UInt batch_start)
    {
        set_block_size(BATCH_SIZE, 1u, 1u);
        auto tid = dispatch_id().x;

        // 1. 初始化梯度为0
        // 仅线程 0 执行 , block 内同步
        $if(tid == 0u)
        {
            for (auto i = 0u; i < NUM_WEIGHT_LAYERS; i++)
            {
                auto in_size  = LAYERS[i];
                auto out_size = LAYERS[i + 1];
                auto w_size   = in_size * out_size;
                $for(j, w_size) { grad_weights[i]->write(j, 0); };
                $for(j, out_size) { grad_biases[i]->write(j, 0); };
            }
        };
        sync_block();

        UInt data_id = batch_start + tid;

        $if(data_id < TRAIN_SIZE)
        {
            ArrayFloat<INPUT_SIZE> X;
            $for(k, INPUT_SIZE)
            {
                X[k] = device_train_images->read(data_id * INPUT_SIZE + k);
            };
            UInt label = device_train_labels->read(data_id);

            ArrayFloat<NUM_NODES> acts;
            ArrayFloat<NUM_NODES> zs;
            ArrayFloat<NUM_NODES> deltas;

            $for(k, INPUT_SIZE)
            {
                acts[k] = X[k];
            };

            // 2. 前向传播
            for (auto i = 0; i < NUM_WEIGHT_LAYERS; i++)
            {
                auto in_start  = layer_offsets[i];
                auto out_start = layer_offsets[i + 1];
                auto in_size   = LAYERS[i];
                auto out_size  = LAYERS[i + 1];

                $for(j, out_size)
                {
                    Float z = biases[i]->read(j);
                    $for(k, in_size)
                    {
                        auto act_in = acts[in_start + k];
                        auto w      = weights[i]->read(j * in_size + k);
                        z += act_in * w;
                    };
                    // store pre-activation for backprop (ReLU derivative needs z)
                    zs[out_start + j]   = z;
                    acts[out_start + j] = i == NUM_WEIGHT_LAYERS - 1 ? 0.0f : ReLU(z);
                };
            }

            // 输出节点用softmax作为激活函数
            auto z_max = zs[OUTPUT_START + 0u];
            $for(c, OUTPUT_SIZE)
            {
                z_max = max(z_max, zs[OUTPUT_START + c]);
            };

            auto sum_exp = def(0.0f);
            $for(c, OUTPUT_SIZE)
            {
                auto z     = zs[OUTPUT_START + c];
                auto exp_z = exp(z - z_max);

                acts[OUTPUT_START + c] = exp_z;
                sum_exp += exp_z;
            };

            $for(c, OUTPUT_SIZE)
            {
                acts[OUTPUT_START + c] = acts[OUTPUT_START + c] / sum_exp;
            };

            // 3. 计算loss & accuracy
            const uint OUTPUT_START = layer_offsets.back();
            Float sample_loss       = 0.0f;
            UInt pred               = 0u;
            Float best              = acts[OUTPUT_START + 0u];
            $for(c, OUTPUT_SIZE)
            {
                auto output_val = acts[OUTPUT_START + c];
                $if(output_val > best)
                {
                    best = output_val;
                    pred = c;
                };

                // 计算输出层delta
                // 激活函数 softmax
                // loss 交叉熵
                deltas[OUTPUT_START + c] = acts[OUTPUT_START + c] - ite(c == label, 1.0f, 0.0f);
            };
            auto target = acts[OUTPUT_START + label];
            loss->atomic(0u).fetch_add(-log(target));
            $if(pred == label) { correct->atomic(0u).fetch_add(1u); };

            // 4. 反向传播
            for (auto i = NUM_WEIGHT_LAYERS - 1; i > 0u; i--)
            {
                auto in_start  = layer_offsets[i];
                auto out_start = layer_offsets[i + 1];
                auto in_size   = LAYERS[i];
                auto out_size  = LAYERS[i + 1];

                $for(j, in_size)
                {
                    Float sum_weighted_delta = 0.0f;
                    $for(k, out_size)
                    {
                        sum_weighted_delta += deltas[out_start + k] * weights[i]->read(k * in_size + j);
                    };

                    Float d_activation   = i == 0 ? 1.0f : ReLU_deriv(zs[in_start + j]);
                    deltas[in_start + j] = sum_weighted_delta * d_activation;
                };
            }

            // 5. 累计 batch 梯度
            for (auto i = 0u; i < NUM_WEIGHT_LAYERS; i++)
            {
                auto in_start  = layer_offsets[i];
                auto out_start = layer_offsets[i + 1];
                auto in_size   = LAYERS[i];
                auto out_size  = LAYERS[i + 1];

                $for(c, out_size)
                {
                    auto d = deltas[out_start + c];
                    $for(j, in_size)
                    {
                        auto input_val = acts[in_start + j];
                        grad_weights[i]->atomic(c * in_size + j).fetch_add(cast<int>(input_val * d * GRAD_SCALE));
                    };
                    grad_biases[i]->atomic(c).fetch_add(cast<int>(d * GRAD_SCALE));
                };
            }
        };

        sync_block();

        // 6. 单线程更新权重（避免对权重做 atomic）
        $if(tid == 0u)
        {
            // 样本的平均梯度
            UInt actual_batch_size = min(BATCH_SIZE, TRAIN_SIZE - batch_start);
            Float inv_n            = 1.0f / cast<float>(actual_batch_size);

            for (auto i = 0u; i < NUM_WEIGHT_LAYERS; i++)
            {
                auto in_size  = LAYERS[i];
                auto out_size = LAYERS[i + 1];
                auto w_size   = in_size * out_size;
                auto b_size   = out_size;

                $for(k, w_size)
                {
                    Float g = cast<float>(grad_weights[i]->read(k)) / GRAD_SCALE * inv_n;
                    auto m  = momentums[i]->read(k) * 0.9f + g;
                    weights[i]->write(k, weights[i]->read(k) - LR * m);
                    momentums[i]->write(k, m);
                };

                $for(k, b_size)
                {
                    Float g = cast<float>(grad_biases[i]->read(k)) / GRAD_SCALE * inv_n;
                    biases[i]->write(k, biases[i]->read(k) - LR * g);
                };
            }
        };
    };

    auto train_shader = device.compile(train_kernel);

    // 训练
    LUISA_INFO("Start Traning!");
    Clock clock;
    for (auto epoch = 0u; epoch < EPOCH_COUNT; epoch++)
    {
        Clock epoch_clock;
        auto zero   = 0.0f;
        auto zero_u = 0u;
        stream
            << loss.copy_from(&zero)
            << correct.copy_from(&zero_u);

        for (auto batch_start = 0u; batch_start < TRAIN_SIZE; batch_start += BATCH_SIZE)
        {
            stream << train_shader(batch_start).dispatch(BATCH_SIZE);
        }
        if (epoch % 1 == 0)
        {
            auto current_loss  = 0.0f;
            auto current_acc   = 0.0f;
            auto current_acc_u = 0u;
            stream
                << loss.copy_to(&current_loss)
                << correct.copy_to(&current_acc_u)
                << synchronize();
            current_loss /= static_cast<float>(TRAIN_SIZE);
            current_acc = static_cast<float>(current_acc_u) / static_cast<float>(TRAIN_SIZE);
            LUISA_INFO("Epoch {}: Loss = {:.8f}, Acc = {:.2f}% time = {:.2f}s", epoch + 1, current_loss, 100.0f * current_acc, epoch_clock.toc() * 1e-3);
        }
    }
    LUISA_INFO("Training {} epoches completed in {:.2f} seconds.", EPOCH_COUNT, clock.toc() * 1e-3);

    // 测试
    auto device_result        = device.create_buffer<float>(OUTPUT_SIZE);
    auto test_loss            = device.create_buffer<float>(1u);
    auto current_test_loss    = 0.0f;
    auto test_correct         = device.create_buffer<uint>(1u);
    auto current_test_correct = 0u;

    Kernel1D test_kernel = [&]()
    {
        set_block_size(256u, 1u, 1u);
        auto data_id = dispatch_id().x;

        $if(data_id < TEST_SIZE)
        {
            ArrayFloat<NUM_NODES> acts;

            // Input
            $for(k, INPUT_SIZE)
            {
                acts[k] = device_test_images->read(data_id * INPUT_SIZE + k);
            };
            UInt label = clamp(device_test_labels->read(data_id), 0u, OUTPUT_SIZE - 1u);

            // 前向传播
            for (auto i = 0; i < NUM_WEIGHT_LAYERS; i++)
            {
                auto in_start  = layer_offsets[i];
                auto out_start = layer_offsets[i + 1];
                auto in_size   = LAYERS[i];
                auto out_size  = LAYERS[i + 1];

                $for(j, out_size)
                {
                    Float z = biases[i]->read(j);
                    $for(k, in_size)
                    {
                        auto act_in = acts[in_start + k];
                        auto w      = weights[i]->read(j * in_size + k);
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
            $if(pred == label) { test_correct->atomic(0u).fetch_add(1u); };
        };
    };
    auto test_shader = device.compile(test_kernel);

    float zero  = 0.0f;
    uint zero_u = 0u;
    stream << test_loss.copy_from(&zero)
           << test_correct.copy_from(&zero_u)
           << synchronize();
    stream << test_shader().dispatch(TEST_SIZE)
           << test_loss.copy_to(&current_test_loss)
           << test_correct.copy_to(&current_test_correct)
           << synchronize();
    current_test_loss /= static_cast<float>(TEST_SIZE);
    LUISA_INFO("Test Average Loss = {}", current_test_loss);
    LUISA_INFO("Test Accuracy = {:.2f}% ({}/{})",
               100.0f * static_cast<float>(current_test_correct) / static_cast<float>(TEST_SIZE),
               current_test_correct,
               TEST_SIZE);

    return 0;
}