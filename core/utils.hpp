#pragma once

#include <vector>

namespace autotune::core
{

double calculateSlope(const std::vector<double>& data,
                      const std::vector<double>& time, size_t windowSize);
double calculateRMSE(const std::vector<double>& data, size_t windowSize);
double calculateMean(const std::vector<double>& data, size_t windowSize);
int scalePwmToRaw(double dutyCycle);
double scaleRawToDuty(int rawPwm);
double linearInterpolateX(double y, double x1, double y1, double x2, double y2);

} // namespace autotune::core
