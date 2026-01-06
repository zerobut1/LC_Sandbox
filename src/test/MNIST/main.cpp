#include <cstdint>
#include <fstream>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/window.h>
#include <luisa/luisa-compute.h>
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

    const uint TRAIN_SIZE  = n_train_labels;
    const uint TEST_SIZE   = n_test_labels;
    const uint INPUT_SIZE  = IMAGE_SIZE * IMAGE_SIZE;
    const uint HIDDEN_SIZE = 16u;
    const uint OUTPUT_SIZE = 10u;
    const float LR         = 0.01f;

    // 初始化权重和偏置
    vector<float> host_w1(INPUT_SIZE * HIDDEN_SIZE); // 输入到隐藏
    vector<float> host_m1(INPUT_SIZE * HIDDEN_SIZE, 0.0f);
    vector<float> host_b1(HIDDEN_SIZE);               // 隐藏层偏置
    vector<float> host_w2(HIDDEN_SIZE * OUTPUT_SIZE); // 隐藏到输出
    vector<float> host_m2(HIDDEN_SIZE * OUTPUT_SIZE, 0.0f);
    vector<float> host_b2(OUTPUT_SIZE);

    // Xavier/Glorot Initialization
    float limit_1 = std::sqrt(6.0f / (static_cast<float>(INPUT_SIZE + HIDDEN_SIZE)));
    float limit_2 = std::sqrt(6.0f / (static_cast<float>(HIDDEN_SIZE + OUTPUT_SIZE)));

    for (auto& v : host_w1)
        v = random_weight(limit_1);
    for (auto& v : host_b1)
        v = 0.0f; // 偏置初始化为 0 通常更好
    for (auto& v : host_w2)
        v = random_weight(limit_2);
    for (auto& v : host_b2)
        v = 0.0f;

    auto w1 = device.create_buffer<float>(INPUT_SIZE * HIDDEN_SIZE);
    auto m1 = device.create_buffer<float>(INPUT_SIZE * HIDDEN_SIZE);
    auto b1 = device.create_buffer<float>(HIDDEN_SIZE);
    auto w2 = device.create_buffer<float>(HIDDEN_SIZE * OUTPUT_SIZE);
    auto m2 = device.create_buffer<float>(HIDDEN_SIZE * OUTPUT_SIZE);
    auto b2 = device.create_buffer<float>(OUTPUT_SIZE);

    stream << w1.copy_from(host_w1.data())
           << m1.copy_from(host_m1.data())
           << b1.copy_from(host_b1.data())
           << w2.copy_from(host_w2.data())
           << m2.copy_from(host_m2.data())
           << b2.copy_from(host_b2.data())
           << synchronize();

    // batch 梯度缓冲区（每个 epoch 清零一次）
    auto grad_w1 = device.create_buffer<float>(INPUT_SIZE * HIDDEN_SIZE);
    auto grad_b1 = device.create_buffer<float>(HIDDEN_SIZE);
    auto grad_w2 = device.create_buffer<float>(HIDDEN_SIZE * OUTPUT_SIZE);
    auto grad_b2 = device.create_buffer<float>(OUTPUT_SIZE);

    auto loss    = device.create_buffer<float>(1);
    auto correct = device.create_buffer<float>(1);

    auto w = device.create_buffer<float>(INPUT_SIZE * HIDDEN_SIZE);

    constexpr uint BATCH_SIZE = 64u;

    Kernel1D train_kernel = [&](UInt batch_start)
    {
        set_block_size(BATCH_SIZE, 1u, 1u);
        auto tid = dispatch_id().x;

        // 清零（仅线程 0 执行），然后 block 内同步
        $if(tid == 0u)
        {
            $for(i, INPUT_SIZE * HIDDEN_SIZE) { grad_w1->write(i, 0.0f); };
            $for(i, HIDDEN_SIZE) { grad_b1->write(i, 0.0f); };
            $for(i, HIDDEN_SIZE * OUTPUT_SIZE) { grad_w2->write(i, 0.0f); };
            $for(i, OUTPUT_SIZE) { grad_b2->write(i, 0.0f); };
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
            ArrayFloat<OUTPUT_SIZE> Y;
            $for(c, OUTPUT_SIZE) { Y[c] = 0.0f; };
            Y[label] = 1.0f;

            // 1) 前向：隐藏层
            ArrayFloat<HIDDEN_SIZE> z1;
            ArrayFloat<HIDDEN_SIZE> a1;
            $for(j, HIDDEN_SIZE)
            {
                z1[j] = b1->read(j);
                $for(k, INPUT_SIZE)
                {
                    z1[j] += X[k] * w1->read(j * INPUT_SIZE + k);
                };
                a1[j] = ReLU(z1[j]);
            };

            // 2) 前向：输出层
            ArrayFloat<OUTPUT_SIZE> z2;
            ArrayFloat<OUTPUT_SIZE> a2;
            $for(c, OUTPUT_SIZE)
            {
                z2[c] = b2->read(c);
                $for(j, HIDDEN_SIZE)
                {
                    z2[c] += a1[j] * w2->read(c * HIDDEN_SIZE + j);
                };
                a2[c] = sigmoid(z2[c]);
            };

            // 3) loss & acc
            Float sample_loss = 0.0f;
            UInt pred         = 0u;
            Float best        = a2[0u];
            $for(c, OUTPUT_SIZE)
            {
                Float diff = a2[c] - Y[c];
                sample_loss += diff * diff;
                $if(a2[c] > best)
                {
                    best = a2[c];
                    pred = c;
                };
            };
            loss->atomic(0u).fetch_add(sample_loss);
            $if(pred == label) { correct->atomic(0u).fetch_add(1.0f); };

            // 4) 反向：输出层（逐类 sigmoid'(z2)）
            ArrayFloat<OUTPUT_SIZE> delta2;
            $for(c, OUTPUT_SIZE)
            {
                Float d_sigmoid_z2 = a2[c] * (1.0f - a2[c]);
                delta2[c]          = 2.0f * (a2[c] - Y[c]) * d_sigmoid_z2;
            };

            // 5) 反向：隐藏层
            ArrayFloat<HIDDEN_SIZE> delta1;
            $for(j, HIDDEN_SIZE)
            {
                Float d_relu_z1 = ReLU_deriv(z1[j]);
                Float sum_delta = 0.0f;
                $for(c, OUTPUT_SIZE)
                {
                    sum_delta += delta2[c] * w2->read(c * HIDDEN_SIZE + j);
                };
                delta1[j] = sum_delta * d_relu_z1;
            };

            // 6) 累计 batch 梯度（用 atomic 做归约）
            $for(c, OUTPUT_SIZE)
            {
                $for(j, HIDDEN_SIZE)
                {
                    grad_w2->atomic(c * HIDDEN_SIZE + j).fetch_add(a1[j] * delta2[c]);
                };
                grad_b2->atomic(c).fetch_add(delta2[c]);
            };

            $for(j, HIDDEN_SIZE)
            {
                $for(k, INPUT_SIZE)
                {
                    grad_w1->atomic(j * INPUT_SIZE + k).fetch_add(X[k] * delta1[j]);
                };
                grad_b1->atomic(j).fetch_add(delta1[j]);
            };
        };

        sync_block();

        // 7) 单线程更新权重（避免对权重做 atomic）
        $if(tid == 0u)
        {
            // 样本的平均梯度
            UInt actual_batch_size = min(BATCH_SIZE, TRAIN_SIZE - batch_start);
            Float inv_n            = 1.0f / cast<float>(actual_batch_size);

            $for(c, OUTPUT_SIZE)
            {
                $for(j, HIDDEN_SIZE)
                {
                    UInt idx = c * HIDDEN_SIZE + j;
                    Float g  = grad_w2->read(idx) * inv_n;
                    auto m   = m2->read(idx) * 0.9f + g;
                    w2->write(idx, w2->read(idx) - LR * m);
                    m2->write(idx, m);
                };
                b2->write(c, b2->read(c) - LR * grad_b2->read(c) * inv_n);
            };

            $for(j, HIDDEN_SIZE)
            {
                $for(k, INPUT_SIZE)
                {
                    UInt idx = j * INPUT_SIZE + k;
                    Float g  = grad_w1->read(idx) * inv_n;
                    auto m   = m1->read(idx) * 0.9f + g;
                    w1->write(idx, w1->read(idx) - LR * m);
                    m1->write(idx, m);
                };
                b1->write(j, b1->read(j) - LR * grad_b1->read(j) * inv_n);
            };
        };
    };

    auto train_shader = device.compile(train_kernel);

    // 训练
    const uint EPOCH_COUNT = 10u;
    LUISA_INFO("Start Traning!");
    Clock clock;
    for (auto epoch = 0u; epoch < EPOCH_COUNT; epoch++)
    {
        Clock epoch_clock;
        auto zero = 0.0f;
        stream
            << loss.copy_from(&zero)
            << correct.copy_from(&zero);

        for (auto batch_start = 0u; batch_start < TRAIN_SIZE; batch_start += BATCH_SIZE)
        {
            stream << train_shader(batch_start).dispatch(BATCH_SIZE);
        }
        if (epoch % 1 == 0)
        {
            auto current_loss = 0.0f;
            auto current_acc  = 0.0f;
            stream
                << loss.copy_to(&current_loss)
                << correct.copy_to(&current_acc)
                << synchronize();
            current_loss /= static_cast<float>(TRAIN_SIZE);
            current_acc /= static_cast<float>(TRAIN_SIZE);
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
            ArrayFloat<INPUT_SIZE> X;
            $for(k, INPUT_SIZE)
            {
                X[k] = device_test_images->read(data_id * INPUT_SIZE + k);
            };

            UInt label = clamp(device_test_labels->read(data_id), 0u, OUTPUT_SIZE - 1u);
            ArrayFloat<OUTPUT_SIZE> Y;
            $for(c, OUTPUT_SIZE) { Y[c] = 0.0f; };
            Y[label] = 1.0f;

            ArrayFloat<HIDDEN_SIZE> z1;
            ArrayFloat<HIDDEN_SIZE> a1;
            $for(j, HIDDEN_SIZE)
            {
                z1[j] = 0.0f;
                $for(k, INPUT_SIZE)
                {
                    z1[j] += X[k] * w1->read(j * INPUT_SIZE + k);
                };
                z1[j] += b1->read(j);
                a1[j] = ReLU(z1[j]);
            };

            ArrayFloat<OUTPUT_SIZE> z2;
            ArrayFloat<OUTPUT_SIZE> a2;
            $for(c, OUTPUT_SIZE)
            {
                z2[c] = b2->read(c);
                $for(j, HIDDEN_SIZE)
                {
                    z2[c] += a1[j] * w2->read(c * HIDDEN_SIZE + j);
                };
                a2[c] = sigmoid(z2[c]);
            };

            // accuracy (argmax)
            UInt pred  = 0u;
            Float best = a2[0u];
            $for(c, OUTPUT_SIZE)
            {
                $if(a2[c] > best)
                {
                    best = a2[c];
                    pred = c;
                };
            };
            $if(pred == label) { test_correct->atomic(0u).fetch_add(1u); };

            Float sample_loss = 0.0f;
            $for(c, OUTPUT_SIZE)
            {
                Float diff = a2[c] - Y[c];
                sample_loss += diff * diff;
            };
            test_loss->atomic(0u).fetch_add(sample_loss);
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