#pragma once

#include "../experiment/step_trigger.hpp"

#include <optional>

namespace autotune::proc
{

struct FopdtParams
{
    double K{};
    double T{};
    double L{};
};

// Identify FOPDT from a recorded step response.
// Returns std::nullopt if not enough information.
std::optional<FopdtParams> identifyFOPDT(const autotune::exp::StepResponse& sr,
                                         double setpoint, int truncateDecimals);

} // namespace autotune::proc
