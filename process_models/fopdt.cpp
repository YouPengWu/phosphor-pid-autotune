#include "fopdt.hpp"

#include "../core/utils.hpp"

#include <cmath>

namespace autotune::process_models
{

static void getFOPDTTemperatures(
    const std::vector<double>& timeSamples,
    const std::vector<double>& temperatureSamples, double stepTime,
    double overrideInitialTemp, double overrideFinalTemp,
    double& initialTemperature, double& finalTemperature)
{
    if (overrideInitialTemp != std::numeric_limits<double>::infinity())
    {
        initialTemperature = overrideInitialTemp;
    }
    else
    {
        initialTemperature = temperatureSamples.front();
        for (size_t i = 0; i < timeSamples.size(); ++i)
        {
            if (timeSamples[i] >= stepTime)
            {
                if (i > 0)
                    initialTemperature = temperatureSamples[i - 1];
                break;
            }
        }
    }

    if (overrideFinalTemp != std::numeric_limits<double>::infinity())
    {
        finalTemperature = overrideFinalTemp;
    }
    else
    {
        finalTemperature = temperatureSamples.back();
    }
}

FOPDTParameters identifyFOPDT(const std::vector<double>& timeSamples,
                              const std::vector<double>& temperatureSamples,
                              double initialPwmRaw, double stepPwmRaw,
                              double stepTime, double overrideInitialTemp,
                              double overrideFinalTemp)
{
    FOPDTParameters params;

    if (timeSamples.size() != temperatureSamples.size() || timeSamples.empty())
        return params;

    double initialTemperature;
    double finalTemperature;
    getFOPDTTemperatures(timeSamples, temperatureSamples, stepTime,
                         overrideInitialTemp, overrideFinalTemp,
                         initialTemperature, finalTemperature);

    if (core::solveLeastSquaresFOPDT(
            timeSamples, temperatureSamples, stepTime, initialTemperature,
            finalTemperature, initialPwmRaw, stepPwmRaw, params.k, params.tau,
            params.theta))
    {
        // Solved successfully
    }

    return params;
}

FOPDTParameters identifyTwoPoint(
    const std::vector<double>& timeSamples,
    const std::vector<double>& temperatureSamples, double initialPwmRaw,
    double stepPwmRaw, double stepTime, double overrideInitialTemp,
    double overrideFinalTemp)
{
    FOPDTParameters params;

    if (timeSamples.size() != temperatureSamples.size() || timeSamples.empty())
        return params;

    double initialTemperature;
    double finalTemperature;
    getFOPDTTemperatures(timeSamples, temperatureSamples, stepTime,
                         overrideInitialTemp, overrideFinalTemp,
                         initialTemperature, finalTemperature);

    double temperatureChange = finalTemperature - initialTemperature;

    double initialDuty = core::scaleRawToDuty(static_cast<int>(initialPwmRaw));
    double stepDuty = core::scaleRawToDuty(static_cast<int>(stepPwmRaw));
    double dutyChange = stepDuty - initialDuty;

    if (std::abs(dutyChange) < 1e-6)
        return params;

    if (core::calculateTwoPointFOPDT(
            timeSamples, temperatureSamples, stepTime, initialTemperature,
            finalTemperature, initialPwmRaw, stepPwmRaw, params.k, params.tau,
            params.theta))
    {
        // Solved successfully
    }

    return params;
}

} // namespace autotune::process_models
