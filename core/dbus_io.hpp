#pragma once

#include <optional>
#include <string>
#include <vector>

namespace autotune::dbusio
{

double readTempCByInput(const std::string& input);
bool writePwmAllByInput(const std::vector<std::string>& inputs, int raw);
std::optional<double> readFanPctByInput(const std::string& input);

} // namespace autotune::dbusio
