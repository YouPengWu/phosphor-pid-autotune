#pragma once

#include <optional>
#include <string>
#include <vector>

namespace autotune::dbusio
{

// Read temperature in degree-C from:
//   /xyz/openbmc_project/sensors/temperature/<input>
// iface: xyz.openbmc_project.Sensor.Value
// prop : Value (double)
double readTempCByInput(const std::string& input);

// Write a raw PWM [0..255] to all fan control objects:
//   /xyz/openbmc_project/control/fanpwm/<input>
// iface: xyz.openbmc_project.Control.FanPwm
// prop : Target (DBus 't' → uint64_t)
// Returns true if all Set operations succeeded.
bool writePwmAllByInput(const std::vector<std::string>& inputs, int raw);

// (Optional) Read current fan percentage [0..100].
//   /xyz/openbmc_project/sensors/fan_pwm/<input>
// iface: xyz.openbmc_project.Sensor.Value
// prop : Value (double)
std::optional<double> readFanPctByInput(const std::string& input);

} // namespace autotune::dbusio
