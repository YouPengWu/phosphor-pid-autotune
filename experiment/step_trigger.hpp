#pragma once

#include "../buildjson/buildjson.hpp"

#include <utility>
#include <vector>

namespace autotune::exp
{

struct StepResponse
{
    // (time index [sec], temperature, input_pwm_raw)
    std::vector<std::tuple<double, double, int>> samples;
    int startDuty{}; // raw
    int endDuty{};   // raw
};

// Trigger a step from base duty and record the response.
StepResponse runStepTrigger(const autotune::Config& cfg, int baseDutyRaw);

} // namespace autotune::exp
