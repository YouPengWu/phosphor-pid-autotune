#include "base_duty.hpp"

#include "../core/dbus_io.hpp"
#include "../core/numeric.hpp"
#include "../core/steady_state.hpp"
#include "../core/time_utils.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace autotune::exp
{

BaseDutyResult runBaseDuty(const autotune::Config& cfg)
{
    BaseDutyResult out{};

    if (!cfg.baseDuty || cfg.fans.empty())
    {
        std::cerr << "[autotune] BaseDuty disabled or no fans.\n";
        return out;
    }

    const auto& e = *cfg.baseDuty;
    const int poll = (cfg.temp.pollIntervalSec > 0)
                         ? cfg.temp.pollIntervalSec
                         : std::max(1, cfg.basic.pollIntervalSec);

    // Regression steady-state detector (slope + RMSE, quantization-aware).
    steady::SteadyStateDetector ss(
        std::max(2, cfg.basic.steadyWindow), static_cast<double>(poll),
        cfg.basic.steadySlopeThresholdPerSec, cfg.basic.steadyRmseThreshold,
        cfg.temp.qStepC);

    // Start from max(minDuty) across fans, clamped to [0,255].
    int duty = cfg.fans.front().minDuty;
    for (const auto& f : cfg.fans)
        duty = std::max(duty, f.minDuty);
    duty = std::clamp(duty, 0, 255);

    const double spTrunc = numeric::truncateDecimals(
        cfg.temp.setpoint, cfg.basic.truncateDecimals);

    // Error band from sensor accuracy + quantization floor (+ optional extra).
    const double quantFloor = cfg.temp.qStepC / std::sqrt(12.0);
    double errBand = std::max(cfg.temp.accuracyC, quantFloor);
    if (cfg.basic.steadySetpointBand > 0.0)
        errBand = std::max(errBand, cfg.basic.steadySetpointBand);

    // Optional CSV log (stream stays open and is flushed line-by-line).
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
                // Header so users can tail the file and understand columns.
                log << "iter,duty,temp_trunc\n";
                log.flush();
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[autotune] BaseDuty log open failed: " << ex.what()
                      << "\n";
        }
    }

    auto applyDuty = [&](int raw) {
        // DBus fan control: write the same raw PWM to all fan "input"
        // endpoints.
        std::vector<std::string> inputs;
        inputs.reserve(cfg.fans.size());
        for (const auto& f : cfg.fans)
            inputs.push_back(f.input);
        (void)dbusio::writePwmAllByInput(inputs, raw);
    };

    int bestDuty = duty;
    double bestErr = std::numeric_limits<double>::infinity();

    int iter = 0;

    while (iter < cfg.basic.maxIterations)
    {
        applyDuty(duty);
        timeutil::sleepSeconds(poll);

        // Read and truncate temperature.
        double temp = dbusio::readTempCByInput(cfg.temp.input);
        temp = numeric::truncateDecimals(temp, cfg.basic.truncateDecimals);

        // Track best-so-far duty by absolute truncated error vs setpoint.
        const double absErr = std::abs(temp - spTrunc);
        if (absErr < bestErr)
        {
            bestErr = absErr;
            bestDuty = duty;
        }

        // Update regression window.
        ss.push(temp);

        // Stream progress immediately (no waiting for experiment end).
        if (log.is_open())
        {
            log << iter << "," << duty << "," << temp << "\n";
            log.flush();
        }

        // Convergence requires BOTH:
        // (A) steady by slope+RMSE, and (B) mean within setpoint ± errBand.
        const bool steady = ss.isSteady();
        const auto ws = ss.stats();
        const bool meanNearSP = (ws.n >= cfg.basic.steadyWindow) &&
                                (std::abs(ws.mean - spTrunc) <= errBand);

        if (steady && meanNearSP)
        {
            out.converged = true;
            out.baseDutyRaw = duty;
            break;
        }

        // Duty update uses errBand instead of a fixed 'tol'.
        if (absErr > errBand)
        {
            duty = (temp > spTrunc) ? std::min(duty + e.stepOutsideTol, 255)
                                    : std::max(duty - e.stepOutsideTol, 0);
        }
        else if (absErr > 0.0) // inside band but not equal
        {
            duty = (temp > spTrunc) ? std::min(duty + e.stepInsideTol, 255)
                                    : std::max(duty - e.stepInsideTol, 0);
        }

        ++iter;
    }

    out.iterations = iter;

    if (!out.converged)
    {
        std::cerr << "[autotune] BaseDuty did not reach steady+setpoint within "
                     "maxIterations="
                  << cfg.basic.maxIterations << ". Using closest duty="
                  << bestDuty << " (|Δ|=" << bestErr << ").\n";
        out.baseDutyRaw = bestDuty;
    }

    // Leave the last chosen duty applied.
    applyDuty(out.baseDutyRaw);
    return out;
}

} // namespace autotune::exp
