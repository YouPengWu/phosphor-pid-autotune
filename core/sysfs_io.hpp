#pragma once

#include <string>
#include <vector>

namespace autotune::sysfs
{

// Write a raw PWM [0..255] to all pwm paths.
void writePwmAll(const std::vector<std::string>& pwmPaths, int raw);

// Read temperature millidegree C from hwmon temp input; return double Celsius.
double readTempC(const std::string& tempInputPath);

// Read integer tach value (RPM) if available; return 0 on failure.
int readTach(const std::string& tachInputPath);

} // namespace autotune::sysfs
