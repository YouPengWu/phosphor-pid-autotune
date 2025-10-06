#pragma once

#include "../buildjson/buildjson.hpp"

#include <optional>
#include <string>
#include <vector>

namespace autotune::exp
{

struct BaseDutyResult
{
    int baseDutyRaw{}; // 0..255
    bool converged{};
    int iterations{};
};

// Search the PWM duty (0..255) that holds temp near setpoint using tolerance
// stepping rules. Applies the same PWM to all fans listed.
BaseDutyResult runBaseDuty(const autotune::Config& cfg);

} // namespace autotune::exp
