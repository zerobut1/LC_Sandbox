#pragma once

#include <random>

namespace Yutrel::RTWeekend
{
    inline double random_double()
    {
        static std::uniform_real_distribution<double> distribution(0.0, 1.0);
        static std::mt19937 generator(std::random_device{}());
        return distribution(generator);
    }

    inline double random_double(double min, double max)
    {
        return std::lerp(min, max, random_double());
    }

    inline int random_int(int min, int max)
    {
        return static_cast<int>(random_double(min, max));
    }
} // namespace Yutrel::RTWeekend