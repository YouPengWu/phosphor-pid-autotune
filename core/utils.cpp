#include "utils.hpp"
#include <algorithm>
#include <cmath>

namespace autotune::core
{

double calculateSlope(const std::vector<double>& data, const std::vector<double>& time, size_t windowSize)
{
    if (data.size() < windowSize || time.size() < windowSize || windowSize < 2) return 0.0;

    size_t start = data.size() - windowSize;
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

    for (size_t i = 0; i < windowSize; ++i)
    {
        double x = time[start + i];
        double y = data[start + i];
        sumX += x; sumY += y; sumXY += x * y; sumX2 += x * x;
    }

    double denominator = windowSize * sumX2 - sumX * sumX;
    return (std::abs(denominator) < 1e-6) ? 0.0 : ((windowSize * sumXY - sumX * sumY) / denominator);
}

double calculateRMSE(const std::vector<double>& data, size_t windowSize)
{
    if (data.size() < windowSize || windowSize == 0) return 0.0;

    double mean = calculateMean(data, windowSize);
    double sumSqDiff = 0.0;
    size_t start = data.size() - windowSize;

    for (size_t i = 0; i < windowSize; ++i)
    {
        double diff = data[start + i] - mean;
        sumSqDiff += diff * diff;
    }

    return std::sqrt(sumSqDiff / windowSize);
}

double calculateMean(const std::vector<double>& data, size_t windowSize)
{
    if (data.size() < windowSize || windowSize == 0) return 0.0;

    size_t start = data.size() - windowSize;
    double sum = 0.0;
    for (size_t i = 0; i < windowSize; ++i) sum += data[start + i];
    
    return sum / windowSize;
}

int scalePwmToRaw(double dutyCycle)
{
    return static_cast<int>(std::clamp(dutyCycle, 0.0, 100.0) * 255.0 / 100.0);
}

double scaleRawToDuty(int rawPwm)
{
    return static_cast<double>(std::clamp(rawPwm, 0, 255)) * 100.0 / 255.0;
}

double linearInterpolateX(double y, double x1, double y1, double x2, double y2)
{
    if (std::abs(y2 - y1) < 1e-9) return x1;
    return x1 + (y - y1) * (x2 - x1) / (y2 - y1);
}

} // namespace autotune::core
