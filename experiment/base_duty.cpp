#include "base_duty.hpp"

#include "../core/numeric.hpp"
#include "../core/steady_state.hpp"
#include "../core/sysfs_io.hpp"
#include "../core/time_utils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>

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
    const int stableN = std::max(1, cfg.basic.stableCount);

    // Use *only* "equal to setpoint" rule for steady-state detection.
    steady::SteadyStateDetector ss(stableN);

    // Start from the tightest min duty across fans (and clamp to [0,255]).
    int duty = cfg.fans.front().minDuty;
    for (const auto& f : cfg.fans)
    {
        duty = std::max(duty, f.minDuty);
    }
    duty = std::clamp(duty, 0, 255);

    const double spTrunc = numeric::truncateDecimals(
        cfg.temp.setpoint, cfg.basic.truncateDecimals);

    // Optional CSV log: iter,duty,tempTrunc
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
                log << "iter,duty,temp_trunc\n";
            }
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
        {
            pwmPaths.push_back(f.pwmPath);
        }
        sysfs::writePwmAll(pwmPaths, raw);
    };

    // Track the "closest to setpoint" duty in case we never reach steady.
    int bestDuty = duty;
    double bestErr = std::numeric_limits<double>::infinity();

    int iter = 0;

    while (iter < cfg.basic.maxIterations)
    {
        // Apply the same duty to all fans.
        applyDuty(duty);

        // Wait poll interval.
        timeutil::sleepSeconds(poll);

        // Read and truncate temperature.
        double temp = sysfs::readTempC(cfg.temp.inputPath);
        temp = numeric::truncateDecimals(temp, cfg.basic.truncateDecimals);

        // Update best-so-far (based on truncated error).
        const double absErr = std::abs(temp - spTrunc);
        if (absErr < bestErr)
        {
            bestErr = absErr;
            bestDuty = duty;
        }

        // Feed detector (only setpoint rule is used to decide steady).
        ss.push(temp, spTrunc);

        // Log CSV if opened.
        if (log.is_open())
        {
            log << iter << "," << duty << "," << temp << "\n";
        }

        // Check steady-at-setpoint only.
        if (ss.isSteadyAtSetpoint())
        {
            out.converged = true;
            out.baseDutyRaw = duty;
            break;
        }

        // Duty update policy (signed):
        // - When outside tol: use stepOutsideTol
        // - When inside tol but not equal: use stepInsideTol
        // Direction:
        //   temp > spTrunc  -> too hot  -> increase duty (fan faster)
        //   temp < spTrunc  -> too cold -> decrease duty (fan slower)
        if (absErr > e.tol)
        {
            if (temp > spTrunc)
            {
                duty = std::min(duty + e.stepOutsideTol, 255);
            }
            else if (temp < spTrunc)
            {
                duty = std::max(duty - e.stepOutsideTol, 0);
            }
        }
        else if (temp != spTrunc)
        {
            if (temp > spTrunc)
            {
                duty = std::min(duty + e.stepInsideTol, 255);
            }
            else /* temp < spTrunc */
            {
                duty = std::max(duty - e.stepInsideTol, 0);
            }
        }
        // If temp == spTrunc: keep duty unchanged and keep accumulating steady
        // counts.

        iter++;
    }

    out.iterations = iter;

    if (!out.converged)
    {
        std::cerr << "[autotune] BaseDuty did not reach steady-at-setpoint "
                     "within maxIterations="
                  << cfg.basic.maxIterations << ". Using closest duty="
                  << bestDuty << " (|Δ|=" << bestErr << ").\n";
        out.baseDutyRaw = bestDuty;
    }

    // Leave the last chosen duty applied.
    applyDuty(out.baseDutyRaw);
    return out;
}

} // namespace autotune::exp
