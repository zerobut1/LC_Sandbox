#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <luisa/core/clock.h>
#include <luisa/core/logging.h>
#include <vector>

using namespace std;

// Sigmoid 激活函数及其导数
double sigmoid(double x)
{
    return 1.0 / (1.0 + exp(-x));
}
double sigmoid_deriv(double x)
{
    double fx = sigmoid(x);
    return fx * (1 - fx);
}

// 随机初始化
double rand_weight()
{
    return ((double)rand() / RAND_MAX) * 2 - 1; // [-1, 1)
}

int main()
{
    srand(time(0));

    // 超参数
    double lr  = 0.1;
    int epochs = 5000;

    // 神经网络结构：2输入-2隐藏-1输出
    // 权重和偏置初始化
    vector<vector<double>> w1(2, vector<double>(2)); // 2x2: 输入到隐藏
    vector<double> b1(2);                            // 2: 隐藏层偏置
    vector<double> w2(2);                            // 2: 隐藏到输出
    double b2 = rand_weight();

    for (auto& row : w1)
        for (auto& v : row)
            v = rand_weight();
    for (auto& v : b1)
        v = rand_weight();
    for (auto& v : w2)
        v = rand_weight();

    // 训练数据（异或）
    vector<vector<double>> X = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    vector<double> Y         = {0, 1, 1, 0};

    // 训练过程
    luisa::Clock clock;
    for (int epoch = 0; epoch < epochs; ++epoch)
    {
        double loss = 0;
        for (int i = 0; i < 4; ++i)
        {
            // 1. 前向传播
            vector<double> z1(2), a1(2);
            for (int j = 0; j < 2; ++j)
            {
                z1[j] = X[i][0] * w1[0][j] + X[i][1] * w1[1][j] + b1[j];
                a1[j] = sigmoid(z1[j]);
            }
            double z2 = a1[0] * w2[0] + a1[1] * w2[1] + b2;
            double a2 = sigmoid(z2);

            // 2. 损失
            loss += (a2 - Y[i]) * (a2 - Y[i]);

            // 3. 反向传播 - 输出层
            double delta2 = 2 * (a2 - Y[i]) * sigmoid_deriv(z2);

            // 4. 反向传播 - 隐藏层
            vector<double> delta1(2);
            for (int j = 0; j < 2; ++j)
                delta1[j] = delta2 * w2[j] * sigmoid_deriv(z1[j]);

            // 5. 更新 - 输出层
            for (int j = 0; j < 2; ++j)
                w2[j] -= lr * a1[j] * delta2;
            b2 -= lr * delta2;

            // 6. 更新 - 隐藏层
            for (int j = 0; j < 2; ++j)
            {
                w1[0][j] -= lr * X[i][0] * delta1[j];
                w1[1][j] -= lr * X[i][1] * delta1[j];
                b1[j] -= lr * delta1[j];
            }
        }
        if (epoch % 500 == 0)
            LUISA_INFO("Epoch {}: Loss = {}", epoch, loss / 4.0f);
    }
    LUISA_INFO("Training completed in {:.2f} seconds.", clock.toc() * 1e-3);

    // 测试
    cout << "===== Test =====" << endl;
    for (int i = 0; i < 4; ++i)
    {
        vector<double> z1(2), a1(2);
        for (int j = 0; j < 2; ++j)
        {
            z1[j] = X[i][0] * w1[0][j] + X[i][1] * w1[1][j] + b1[j];
            a1[j] = sigmoid(z1[j]);
        }
        double z2 = a1[0] * w2[0] + a1[1] * w2[1] + b2;
        double a2 = sigmoid(z2);
        LUISA_INFO("Input: ({}, {}), Predicted: {:.4f}, Ground Truth: {}", X[i][0], X[i][1], a2, Y[i]);
    }
    return 0;
}