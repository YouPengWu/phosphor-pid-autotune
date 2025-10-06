#pragma once

#include <cmath>
#include <cstdint>

namespace autotune::numeric
{

// Truncate a floating value to N decimals without rounding.
inline double truncateDecimals(double value, int decimals)
{
    if (decimals <= 0)
    {
        return std::floor(value);
    }
    const double scale = std::pow(10.0, decimals);
    return std::floor(value * scale) / scale;
}

// Convert percent [0..100] to PWM raw [0..255].
inline int percentToPwm(double percent)
{
    if (percent < 0.0)
        percent = 0.0;
    if (percent > 100.0)
        percent = 100.0;
    return static_cast<int>(std::lround(percent / 100.0 * 255.0));
}

// Convert PWM raw [0..255] to percent [0..100].
inline double pwmToPercent(int pwm)
{
    if (pwm < 0)
        pwm = 0;
    if (pwm > 255)
        pwm = 255;
    return (static_cast<double>(pwm) / 255.0) * 100.0;
}

} // namespace autotune::numeric
