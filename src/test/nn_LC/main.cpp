#include <array>
#include <luisa/dsl/sugar.h>
#include <luisa/luisa-compute.h>
#include <random>

using namespace luisa;
using namespace luisa::compute;

const uint SEED = 20120712u;

float random_float()
{
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    static std::default_random_engine generator(SEED);
    return distribution(generator);
}

float random_weight()
{
    return random_float() * 2.0f - 1.0f; // [-1, 1)
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

    // 训练数据（异或）
    constexpr uint TRAIN_SIZE = 4u;
    std::array<float2, TRAIN_SIZE> host_x{
        make_float2(0.0f, 0.0f),
        make_float2(0.0f, 1.0f),
        make_float2(1.0f, 0.0f),
        make_float2(1.0f, 1.0f)};
    std::array<float, TRAIN_SIZE> host_y{0.0f, 1.0f, 1.0f, 0.0f};
    auto train_x = device.create_buffer<float2>(TRAIN_SIZE);
    auto train_y = device.create_buffer<float>(TRAIN_SIZE);

    stream << train_x.copy_from(host_x.data())
           << train_y.copy_from(host_y.data())
           << synchronize();

    const uint INPUT_SIZE  = 2;
    const uint HIDDEN_SIZE = 2;
    const float LR         = 0.5f;

    // 初始化权重和偏置
    vector<float> host_w1(INPUT_SIZE * HIDDEN_SIZE); // 输入到隐藏
    vector<float> host_b1(HIDDEN_SIZE);              // 隐藏层偏置
    vector<float> host_w2(HIDDEN_SIZE);              // 隐藏到输出
    vector<float> host_b2(1);

    for (auto& v : host_w1)
        v = random_weight();
    for (auto& v : host_b1)
        v = random_weight();
    for (auto& v : host_w2)
        v = random_weight();
    host_b2[0] = random_weight();

    auto w1 = device.create_buffer<float>(INPUT_SIZE * HIDDEN_SIZE);
    auto b1 = device.create_buffer<float>(HIDDEN_SIZE);
    auto w2 = device.create_buffer<float>(HIDDEN_SIZE);
    auto b2 = device.create_buffer<float>(1);

    stream << w1.copy_from(host_w1.data())
           << b1.copy_from(host_b1.data())
           << w2.copy_from(host_w2.data())
           << b2.copy_from(host_b2.data())
           << synchronize();

    // batch 梯度缓冲区（每个 epoch 清零一次）
    auto grad_w1 = device.create_buffer<float>(INPUT_SIZE * HIDDEN_SIZE);
    auto grad_b1 = device.create_buffer<float>(HIDDEN_SIZE);
    auto grad_w2 = device.create_buffer<float>(HIDDEN_SIZE);
    auto grad_b2 = device.create_buffer<float>(1);

    auto loss         = device.create_buffer<float>(1);
    auto current_loss = 0.0f;

    constexpr uint BLOCK_SIZE = 32u;

    Kernel1D train_kernel = [&]()
    {
        set_block_size(BLOCK_SIZE, 1u, 1u);
        UInt tid = dispatch_id().x;

        // 清零（仅线程 0 执行），然后 block 内同步
        $if(tid == 0u)
        {
            $for(i, INPUT_SIZE * HIDDEN_SIZE) { grad_w1->write(i, 0.0f); };
            $for(i, HIDDEN_SIZE) { grad_b1->write(i, 0.0f); };
            $for(i, HIDDEN_SIZE) { grad_w2->write(i, 0.0f); };
            grad_b2->write(0u, 0.0f);
            loss->write(0u, 0.0f);
        };
        sync_block();

        // 仅前 TRAIN_SIZE 个线程处理样本；其余线程必须空转但仍参与 sync_block()
        $if(tid < TRAIN_SIZE)
        {
            UInt data_id = tid;
            Float2 X     = train_x->read(data_id);
            Float Y      = train_y->read(data_id);

            // 1) 前向：隐藏层
            ArrayFloat<HIDDEN_SIZE> z1;
            ArrayFloat<HIDDEN_SIZE> a1;
            $for(j, HIDDEN_SIZE)
            {
                z1[j] = X.x * w1->read(j * INPUT_SIZE + 0u) + X.y * w1->read(j * INPUT_SIZE + 1u) + b1->read(j);
                a1[j] = sigmoid(z1[j]);
            };

            // 2) 前向：输出层
            Float z2 = a1[0] * w2->read(0u) + a1[1] * w2->read(1u) + b2->read(0u);
            Float a2 = sigmoid(z2);

            // 3) loss
            Float sample_loss = (a2 - Y) * (a2 - Y);
            loss->atomic(0u).fetch_add(sample_loss);

            // 4) 反向：输出层（复用 a2 计算 sigmoid'(z2)）
            Float d_sigmoid_z2 = a2 * (1.0f - a2);
            Float delta2       = 2.0f * (a2 - Y) * d_sigmoid_z2;

            // 5) 反向：隐藏层
            ArrayFloat<HIDDEN_SIZE> delta1;
            $for(j, HIDDEN_SIZE)
            {
                Float d_sigmoid_z1 = a1[j] * (1.0f - a1[j]);
                delta1[j]          = delta2 * w2->read(j) * d_sigmoid_z1;
            };

            // 6) 累计 batch 梯度（用 atomic 做归约）
            $for(j, HIDDEN_SIZE)
            {
                grad_w2->atomic(j).fetch_add(a1[j] * delta2);
            };
            grad_b2->atomic(0u).fetch_add(delta2);

            $for(j, HIDDEN_SIZE)
            {
                grad_w1->atomic(j * INPUT_SIZE + 0u).fetch_add(X.x * delta1[j]);
                grad_w1->atomic(j * INPUT_SIZE + 1u).fetch_add(X.y * delta1[j]);
                grad_b1->atomic(j).fetch_add(delta1[j]);
            };
        };

        sync_block();

        // 7) 单线程更新权重（避免对权重做 atomic）
        $if(tid == 0u)
        {
            // 样本的平均梯度
            Float inv_n = 1.0f / static_cast<float>(TRAIN_SIZE);

            $for(j, HIDDEN_SIZE)
            {
                Float g = grad_w2->read(j) * inv_n;
                w2->write(j, w2->read(j) - LR * g);
            };
            b2->write(0u, b2->read(0u) - LR * grad_b2->read(0u) * inv_n);

            $for(j, HIDDEN_SIZE)
            {
                $for(k, INPUT_SIZE)
                {
                    UInt idx = j * INPUT_SIZE + k;
                    Float g  = grad_w1->read(idx) * inv_n;
                    w1->write(idx, w1->read(idx) - LR * g);
                };
                b1->write(j, b1->read(j) - LR * grad_b1->read(j) * inv_n);
            };

            // 写回平均 loss，便于主机侧打印
            loss->write(0u, loss->read(0u) * inv_n);
        };
    };

    auto train_shader = device.compile(train_kernel);

    const uint EPOCH_COUNT = 5000u;
    Clock clock;
    for (auto epoch = 0u; epoch < EPOCH_COUNT; epoch++)
    {
        stream << train_shader().dispatch(BLOCK_SIZE);
        if (epoch % 500 == 0)
        {
            stream << loss.copy_to(&current_loss) << synchronize();
            LUISA_INFO("Epoch {}: Loss = {}", epoch, current_loss);
        }
    }
    stream << loss.copy_to(&current_loss)
           << synchronize();
    LUISA_INFO("Epoch {}: Loss = {}", EPOCH_COUNT, current_loss);
    LUISA_INFO("Training completed in {:.2f} seconds.", clock.toc() * 1e-3);

    // 测试
    auto result = device.create_buffer<float>(1);

    Kernel1D test_kernel = [&](Float2 X)
    {
        // 前向传播
        ArrayFloat<HIDDEN_SIZE> z1;
        ArrayFloat<HIDDEN_SIZE> a1;
        $for(j, HIDDEN_SIZE)
        {
            z1[j] = X.x * w1->read(j * INPUT_SIZE + 0) + X.y * w1->read(j * INPUT_SIZE + 1) + b1->read(j);
            a1[j] = sigmoid(z1[j]);
        };
        auto z2 = a1[0] * w2->read(0) + a1[1] * w2->read(1) + b2->read(0);
        auto a2 = sigmoid(z2);
        result->write(0, a2);
    };
    auto test_shader = device.compile(test_kernel);

    for (auto i = 0; i < 4; ++i)
    {
        float2 input = host_x[i];
        float output;
        stream << test_shader(input).dispatch(1);
        stream << result.copy_to(&output) << synchronize();
        LUISA_INFO("Input: ({}, {}), Predicted: {:.4f}, Ground Truth: {}", input.x, input.y, output, host_y[i]);
    }

    return 0;
}