#include "fopdt.hpp"

#include "../core/utils.hpp"

#include <algorithm>
#include <cmath>

namespace autotune::process_models
{

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

    double finalTemperature;
    if (overrideFinalTemp != std::numeric_limits<double>::infinity())
    {
        finalTemperature = overrideFinalTemp;
    }
    else
    {
        finalTemperature = temperatureSamples.back();
    }
    double temperatureChange = finalTemperature - initialTemperature;

    double initialDuty = core::scaleRawToDuty(static_cast<int>(initialPwmRaw));
    double stepDuty = core::scaleRawToDuty(static_cast<int>(stepPwmRaw));
    double dutyChange = stepDuty - initialDuty;

    if (std::abs(dutyChange) < 1e-6)
        return params;

    params.k = temperatureChange / dutyChange;

    // t_p = theta - tau * ln(1 - p)
    // p1 = 0.283 (28.3%), p2 = 0.632 (63.2%)
    double tempAt28Percent = initialTemperature + 0.283 * temperatureChange;
    double tempAt63Percent = initialTemperature + 0.632 * temperatureChange;

    double timeAt28Percent = -1.0;
    double timeAt63Percent = -1.0;
    bool found28 = false;
    bool found63 = false;
    bool increasing = (temperatureChange > 0);

    for (size_t i = 0; i < timeSamples.size(); ++i)
    {
        if (timeSamples[i] < stepTime)
            continue;

        double currentTemp = temperatureSamples[i];

        if (!found28)
        {
            if ((increasing && currentTemp >= tempAt28Percent) ||
                (!increasing && currentTemp <= tempAt28Percent))
            {
                timeAt28Percent =
                    (i > 0) ? core::linearInterpolateX(
                                  tempAt28Percent, timeSamples[i - 1],
                                  temperatureSamples[i - 1], timeSamples[i],
                                  temperatureSamples[i])
                            : timeSamples[i];
                found28 = true;
            }
        }

        if (!found63)
        {
            if ((increasing && currentTemp >= tempAt63Percent) ||
                (!increasing && currentTemp <= tempAt63Percent))
            {
                timeAt63Percent =
                    (i > 0) ? core::linearInterpolateX(
                                  tempAt63Percent, timeSamples[i - 1],
                                  temperatureSamples[i - 1], timeSamples[i],
                                  temperatureSamples[i])
                            : timeSamples[i];
                found63 = true;
            }
        }

        if (found28 && found63)
            break;
    }

    if (found28 && found63)
    {
        double relativeTime28 = timeAt28Percent - stepTime;
        double relativeTime63 = timeAt63Percent - stepTime;

        params.tau = 1.5 * (relativeTime63 - relativeTime28);
        params.theta = relativeTime63 - params.tau;

        if (params.theta < 0)
            params.theta = 0;
    }

    return params;
}

} // namespace autotune::process_models
