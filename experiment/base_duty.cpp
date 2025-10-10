#include "base_duty.hpp"

#include "../core/numeric.hpp"
#include "../core/steady_state.hpp"
#include "../core/sysfs_io.hpp"
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
    const int poll = std::max(1, cfg.basic.pollIntervalSec);

    // Steady-state detector: slope + RMSE with quantization floors.
    steady::SteadyStateDetector ss(
        std::max(2, cfg.basic.steadyWindow), static_cast<double>(poll),
        cfg.basic.steadySlopeThresholdPerSec, cfg.basic.steadyRmseThreshold,
        cfg.temp.qStepC /* °C/LSB */
    );

    // Start from max(minDuty) across fans, clamped to [0,255].
    int duty = cfg.fans.front().minDuty;
    for (const auto& f : cfg.fans)
        duty = std::max(duty, f.minDuty);
    duty = std::clamp(duty, 0, 255);

    const double spTrunc = numeric::truncateDecimals(
        cfg.temp.setpoint, cfg.basic.truncateDecimals);

    // Dynamic error band from sensor accuracy + quantization floor.
    const double quantFloor = cfg.temp.qStepC / std::sqrt(12.0);
    double errBand = std::max(cfg.temp.accuracyC, quantFloor);
    if (cfg.basic.steadySetpointBand > 0.0)
        errBand = std::max(errBand, cfg.basic.steadySetpointBand);

    // Optional CSV log: iter,duty,temp_trunc
    std::ofstream log;
    if (!e.logPath.empty())
    {
        try
        {
            std::filesystem::create_directories(
                std::filesystem::path(e.logPath).parent_path());
            log.open(e.logPath, std::ios::out | std::ios::trunc);
            if (log.is_open())
                log << "iter,duty,temp_trunc\n";
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[autotune] BaseDuty log open failed: " << ex.what()
                      << "\n";
        }
    }

    auto applyDuty = [&](int raw) {
        std::vector<std::string> pwmPaths;
        pwmPaths.reserve(cfg.fans.size());
        for (const auto& f : cfg.fans)
            pwmPaths.push_back(f.pwmPath);
        sysfs::writePwmAll(pwmPaths, raw);
    };

    int bestDuty = duty;
    double bestErr = std::numeric_limits<double>::infinity();

    int iter = 0;

    while (iter < cfg.basic.maxIterations)
    {
        applyDuty(duty);
        timeutil::sleepSeconds(poll);

        double temp = sysfs::readTempC(cfg.temp.inputPath);
        temp = numeric::truncateDecimals(temp, cfg.basic.truncateDecimals);

        const double absErr = std::abs(temp - spTrunc);
        if (absErr < bestErr)
        {
            bestErr = absErr;
            bestDuty = duty;
        }

        ss.push(temp);

        if (log.is_open())
            log << iter << "," << duty << "," << temp << "\n";

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

        // Duty update uses errBand instead of tol.
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
        std::cerr
            << "[autotune] BaseDuty did not reach steady+setpoint within maxIterations="
            << cfg.basic.maxIterations << ". Using closest duty=" << bestDuty
            << " (|Δ|=" << bestErr << ").\n";
        out.baseDutyRaw = bestDuty;
    }

    applyDuty(out.baseDutyRaw);
    return out;
}

} // namespace autotune::exp
