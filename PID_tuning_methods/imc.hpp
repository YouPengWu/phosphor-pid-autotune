#pragma once

#include "../buildjson/buildjson.hpp"
#include "../process_models/fopdt.hpp"

#include <map>
#include <string>
#include <vector>

#include <optional>
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

struct ImcResult
{
    double epsilon;
    double ratio; // epsilon / theta
    std::string type; // "PID", "PI", "Improved PI"
    PidGains gains;
};

// IMC PID tuning for multiple epsilon factors; returns list of results.
std::vector<ImcResult> imcFromFopdt(
    const autotune::proc::FopdtParams& p,
    const std::vector<double>& epsilonFactors);

} // namespace autotune::tuning
