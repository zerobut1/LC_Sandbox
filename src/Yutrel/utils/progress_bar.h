#pragma once

#include <chrono>

namespace Yutrel
{
// credit: https://github.com/AirGuanZ/agz-utils/blob/master/include/agz-utils/console/pbar.h#L137
class ProgressBar
{
public:
    static constexpr auto complete_char   = '=';
    static constexpr auto heading_char    = '>';
    static constexpr auto incomplete_char = ' ';
    using clock_type                      = std::chrono::steady_clock;

private:
    double _progress;
    uint32_t _width;
    clock_type::time_point _start;

public:
    explicit ProgressBar(uint32_t width = 50u) noexcept;
    void reset() noexcept;
    void update(double progress) noexcept;
    void done() noexcept;
};

} // namespace Yutrel
