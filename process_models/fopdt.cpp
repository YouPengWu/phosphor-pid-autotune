#include "fopdt.hpp"

#include "../core/utils.hpp"
#include "../solvers/least_squares.hpp"
#include "../solvers/nelder_mead.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

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

    params.k = temperatureChange / dutyChange;

    double temp28 = initialTemperature + 0.283 * temperatureChange;
    double temp63 = initialTemperature + 0.632 * temperatureChange;

    double time28 = -1.0;
    double time63 = -1.0;

    for (size_t i = 1; i < timeSamples.size(); ++i)
    {
        if (timeSamples[i] < stepTime)
            continue;

        double t1 = timeSamples[i - 1];
        double y1 = temperatureSamples[i - 1];
        double t2 = timeSamples[i];
        double y2 = temperatureSamples[i];

        if (time28 < 0)
        {
            if ((temperatureChange > 0 && y1 < temp28 && y2 >= temp28) ||
                (temperatureChange < 0 && y1 > temp28 && y2 <= temp28))
            {
                time28 = core::linearInterpolateX(temp28, t1, y1, t2, y2);
            }
        }

        if (time63 < 0)
        {
            if ((temperatureChange > 0 && y1 < temp63 && y2 >= temp63) ||
                (temperatureChange < 0 && y1 > temp63 && y2 <= temp63))
            {
                time63 = core::linearInterpolateX(temp63, t1, y1, t2, y2);
            }
        }

        if (time28 > 0 && time63 > 0)
            break;
    }

    if (time28 > 0 && time63 > 0)
    {
        params.tau = 1.5 * (time63 - time28);
        params.theta = (time63 - stepTime) - params.tau;
        if (params.theta < 0)
            params.theta = 0;
    }

    return params;
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

    double temperatureChange = finalTemperature - initialTemperature;
    double initialDuty = core::scaleRawToDuty(static_cast<int>(initialPwmRaw));
    double stepDuty = core::scaleRawToDuty(static_cast<int>(stepPwmRaw));
    double dutyChange = stepDuty - initialDuty;

    if (std::abs(dutyChange) < 1e-6)
        return params;

    params.k = temperatureChange / dutyChange;

    std::vector<double> xLog;
    std::vector<double> yLog;

    for (size_t i = 0; i < timeSamples.size(); ++i)
    {
        if (timeSamples[i] < stepTime)
            continue;

        double currentTemp = temperatureSamples[i];
        double yNorm = (currentTemp - initialTemperature) / temperatureChange;

        // Threshold 10% to 90%
        if (yNorm > 0.1 && yNorm < 0.9)
        {
            double val = 1.0 - yNorm;
            if (val > 1e-9)
            {
                // Linearization: ln(1 - yNorm) = - (t - theta) / tau
                // x = -ln(1 - yNorm)
                double lx = -std::log(val);
                // y = t_rel
                double ly = timeSamples[i] - stepTime;

                xLog.push_back(lx);
                yLog.push_back(ly);
            }
        }
    }

    auto res = solvers::LeastSquares::solveLinearRegression(xLog, yLog);
    if (res.valid && res.slope > 1e-9)
    {
        params.tau = res.slope;
        params.theta = res.intercept;
        if (params.theta < 0)
            params.theta = 0;
    }

    return params;
}

// Helper: SSD Cost Function
static double calculateSSD(const std::vector<double>& time,
                           const std::vector<double>& temp, double k_process,
                           double tau, double theta, double stepTime,
                           double initialTemp)
{
    double ssd = 0.0;
    for (size_t i = 0; i < time.size(); ++i)
    {
        double t = time[i];
        double y_meas = temp[i];

        // Model: y(t) given k_process (Temperature Gain of the step)
        double y_pred = initialTemp;
        if (t >= stepTime + theta)
        {
            y_pred =
                initialTemp +
                k_process * (1.0 - std::exp(-(t - stepTime - theta) / tau));
        }

        ssd += (y_meas - y_pred) * (y_meas - y_pred);
    }
    return ssd;
}

FOPDTParameters identifyOptimization(
    const std::vector<double>& timeSamples,
    const std::vector<double>& temperatureSamples, double initialPwmRaw,
    double stepPwmRaw, double stepTime, double overrideInitialTemp,
    double overrideFinalTemp)
{
    FOPDTParameters params;

    // 1. Get Initial Guesses using TwoPoint
    params = identifyTwoPoint(timeSamples, temperatureSamples, initialPwmRaw,
                              stepPwmRaw, stepTime, overrideInitialTemp,
                              overrideFinalTemp);

    // Initial estimation of T_init
    double initialTemperature, finalTemperature;
    getFOPDTTemperatures(timeSamples, temperatureSamples, stepTime,
                         overrideInitialTemp, overrideFinalTemp,
                         initialTemperature, finalTemperature);

    double tempChange = finalTemperature - initialTemperature;
    double initialDuty = core::scaleRawToDuty(static_cast<int>(initialPwmRaw));
    double stepDuty = core::scaleRawToDuty(static_cast<int>(stepPwmRaw));
    double dutyChange = stepDuty - initialDuty;

    if (std::abs(dutyChange) < 1e-6)
        return params;

    // Initial Params Vector: [K_step (Total Temp Change), Tau, Theta]
    double k_step_guess = tempChange;
    double tau_guess = (params.tau > 0) ? params.tau : 10.0;
    double theta_guess = (params.theta > 0) ? params.theta : 1.0;

    std::vector<double> initialParams = {k_step_guess, tau_guess, theta_guess};

    // Cost Function wrapper
    auto costFunc = [&](const std::vector<double>& p) -> double {
        double k_s = p[0];
        double t_const = p[1];
        double t_delay = p[2];

        // Constraint Penalties
        if (t_const < 0.1 || t_delay < 0.0)
            return 1e15;

        return calculateSSD(timeSamples, temperatureSamples, k_s, t_const,
                            t_delay, stepTime, initialTemperature);
    };

    // Run Optimization (3 dimensions)
    auto bestParams = solvers::NelderMead::solve(initialParams, costFunc, 200);

    // Map back to FOPDTParameters
    double best_k_step = bestParams[0];
    double best_tau = bestParams[1];
    double best_theta = bestParams[2];

    params.k = best_k_step / dutyChange;
    params.tau = best_tau;
    params.theta = best_theta;

    return params;
}

} // namespace autotune::process_models
