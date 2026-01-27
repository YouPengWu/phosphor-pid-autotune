#pragma once

#include <limits>
#include <string>
#include <vector>

namespace autotune::process_models
{

struct FOPDTParameters
{
    double k = 0.0;
    double tau = 0.0;
    double theta = 0.0;
};

/**
 * @brief Identify FOPDT parameters from step response data.
 * @param time Time vector
 * @param temp Temperature vector
 * @param initialPwm PWM before step
 * @param stepPwm PWM after step
 * @param stepTime Time when step occurred
 */
FOPDTParameters identifyFOPDT(
    const std::vector<double>& time, const std::vector<double>& temp,
    double initialPwm, double stepPwm, double stepTime,
    double overrideInitialTemp = std::numeric_limits<double>::infinity(),
    double overrideFinalTemp = std::numeric_limits<double>::infinity());

} // namespace autotune::process_models
