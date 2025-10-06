#pragma once

#include "../buildjson/buildjson.hpp"
#include "../process_models/fopdt.hpp"

#include <map>
#include <string>
#include <vector>

namespace autotune::tuning
{

struct PidGains
{
    double Kp{};
    double Ki{};
    double Kd{};
};

// IMC PID tuning for multiple lambda factors; returns map lambda->gains.
std::map<double, PidGains> imcFromFopdt(
    const autotune::proc::FopdtParams& p,
    const std::vector<double>& lambdaFactors);

} // namespace autotune::tuning
