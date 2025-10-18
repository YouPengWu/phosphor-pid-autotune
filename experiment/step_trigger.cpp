#include "step_trigger.hpp"

#include "../core/dbus_io.hpp"
#include "../core/numeric.hpp"
#include "../core/steady_state.hpp"
#include "../core/time_utils.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
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

    // Gather DBus fan "inputs" so we can broadcast the PWM step to all of them.
    std::vector<std::string> inputs;
    inputs.reserve(cfg.fans.size());
    for (const auto& f : cfg.fans)
        inputs.push_back(f.input);

    auto applyDuty = [&](int raw) {
        (void)dbusio::writePwmAllByInput(inputs, raw);
    };

    // Regression-based steady detector (quantization-aware).
    steady::SteadyStateDetector ss(
        std::max(2, cfg.basic.steadyWindow), static_cast<double>(poll),
        cfg.basic.steadySlopeThresholdPerSec, cfg.basic.steadyRmseThreshold,
        cfg.temp.qStepC);

    const double spTrunc = numeric::truncateDecimals(
        cfg.temp.setpoint, cfg.basic.truncateDecimals);

    // Pre-step requirement needs a setpoint band.
    const double quantFloor = cfg.temp.qStepC / std::sqrt(12.0);
    double errBand = std::max(cfg.temp.accuracyC, quantFloor);
    if (cfg.basic.steadySetpointBand > 0.0)
        errBand = std::max(errBand, cfg.basic.steadySetpointBand);

    int pwm = std::clamp(baseDutyRaw, 0, 255);
    applyDuty(pwm);

    // Optional CSV log for step response; write as we go.
    std::ofstream log;
    if (!e.logPath.empty())
    {
        try
        {
            std::filesystem::create_directories(
                std::filesystem::path(e.logPath).parent_path());
            log.open(e.logPath, std::ios::out | std::ios::trunc);
            if (log.is_open())
            {
                // FIX: header must match the 7 fields we log each line
                log << "t_index,temp_trunc,pwm,slope,rmse,n,mean\n";
                log.flush();
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[autotune] StepTrigger log open failed: " << ex.what()
                      << "\n";
        }
    }

    int i = 0;
    bool jumped = false;

    while (i < cfg.basic.maxIterations)
    {
        timeutil::sleepSeconds(poll);

        double temp = dbusio::readTempCByInput(cfg.temp.input);
        temp = numeric::truncateDecimals(temp, cfg.basic.truncateDecimals);

        out.samples.emplace_back(i, temp, pwm);
        ss.push(temp);

        // Stream each sample with up-to-date regression stats.
        if (log.is_open())
        {
            const auto st = ss.stats(); // slope, rmse, mean, n
            log << i << "," << temp << "," << pwm << ","
                << st.slope << "," << st.rmse << ","
                << st.n << "," << st.mean << "\n";
            log.flush();
        }

        if (!jumped)
        {
            // Pre-step condition: steady AND near setpoint band.
            const bool steady = ss.isSteady();
            const auto ws = ss.stats();
            const bool meanNearSP = (ws.n >= cfg.basic.steadyWindow) &&
                                    (std::abs(ws.mean - spTrunc) <= errBand);

            if (steady && meanNearSP)
            {
                // Apply the step once pre-step conditions are satisfied.
                pwm = std::clamp(baseDutyRaw + e.stepDuty, 0, 255);
                applyDuty(pwm);

                // Reset detector; post-step uses steady-only.
                ss.reset();
                jumped = true;
            }
        }
        else
        {
            // Post-step stop condition: steady only (the mean can be away from SP).
            if (ss.isSteady())
                break;
        }

        ++i;
    }

    out.startDuty = baseDutyRaw;
    out.endDuty = out.samples.empty() ? pwm : std::get<2>(out.samples.back());
    return out;
}

} // namespace autotune::exp
