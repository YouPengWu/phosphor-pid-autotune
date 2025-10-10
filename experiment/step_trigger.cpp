#include "step_trigger.hpp"

#include "../core/numeric.hpp"
#include "../core/steady_state.hpp"
#include "../core/sysfs_io.hpp"
#include "../core/time_utils.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace autotune::exp
{

StepResponse runStepTrigger(const autotune::Config& cfg, int baseDutyRaw)
{
    StepResponse out{};

    if (!cfg.stepTrigger || cfg.fans.empty())
    {
        std::cerr << "[autotune] StepTrigger disabled or no fans.\n";
        return out;
    }

    const auto& e = *cfg.stepTrigger;
    const int poll = std::max(1, cfg.basic.pollIntervalSec);

    std::vector<std::string> pwmPaths;
    pwmPaths.reserve(cfg.fans.size());
    for (const auto& f : cfg.fans)
        pwmPaths.push_back(f.pwmPath);

    auto applyDuty = [&](int raw) { sysfs::writePwmAll(pwmPaths, raw); };

    // Regression-based steady detector (quantization-aware).
    steady::SteadyStateDetector ss(
        std::max(2, cfg.basic.steadyWindow), static_cast<double>(poll),
        cfg.basic.steadySlopeThresholdPerSec, cfg.basic.steadyRmseThreshold,
        cfg.temp.qStepC /* °C/LSB */
    );

    const double spTrunc = numeric::truncateDecimals(
        cfg.temp.setpoint, cfg.basic.truncateDecimals);

    // Dynamic error band for pre-step condition.
    const double quantFloor = cfg.temp.qStepC / std::sqrt(12.0);
    double errBand = std::max(cfg.temp.accuracyC, quantFloor);
    if (cfg.basic.steadySetpointBand > 0.0)
        errBand = std::max(errBand, cfg.basic.steadySetpointBand);

    int pwm = std::clamp(baseDutyRaw, 0, 255);
    applyDuty(pwm);

    int i = 0;
    bool jumped = false;

    while (i < cfg.basic.maxIterations)
    {
        timeutil::sleepSeconds(poll);

        double temp = sysfs::readTempC(cfg.temp.inputPath);
        temp = numeric::truncateDecimals(temp, cfg.basic.truncateDecimals);

        out.samples.emplace_back(i, temp, pwm);
        ss.push(temp);

        if (!jumped)
        {
            // Pre-step requirement:
            // (A) steady by slope+RMSE AND (B) mean within setpoint ± errBand.
            const bool steady = ss.isSteady();
            const auto ws = ss.stats();
            const bool meanNearSP = (ws.n >= cfg.basic.steadyWindow) &&
                                    (std::abs(ws.mean - spTrunc) <= errBand);

            if (steady && meanNearSP)
            {
                // Apply step once pre-step conditions are met.
                pwm = std::clamp(baseDutyRaw + e.stepDuty, 0, 255);
                applyDuty(pwm);

                // Reset detector; post-step only requires steady state.
                ss.reset();
                jumped = true;
            }
        }
        else
        {
            // Post-step stop criterion: steady only (we expect offset from SP).
            if (ss.isSteady())
            {
                break;
            }
        }

        ++i;
    }

    out.startDuty = baseDutyRaw;
    out.endDuty = out.samples.empty() ? pwm : std::get<2>(out.samples.back());
    return out;
}

} // namespace autotune::exp
