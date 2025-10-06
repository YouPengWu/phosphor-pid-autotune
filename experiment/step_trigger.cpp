#include "step_trigger.hpp"

#include "../core/numeric.hpp"
#include "../core/steady_state.hpp"
#include "../core/sysfs_io.hpp"
#include "../core/time_utils.hpp"

#include <algorithm>
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
    const int stableN = std::max(1, cfg.basic.stableCount);

    std::vector<std::string> pwmPaths;
    pwmPaths.reserve(cfg.fans.size());
    for (const auto& f : cfg.fans)
    {
        pwmPaths.push_back(f.pwmPath);
    }

    auto applyDuty = [&](int raw) { sysfs::writePwmAll(pwmPaths, raw); };

    // Steady-state detector using "equal to previous" rule both before and
    // after step.
    steady::SteadyStateDetector ssPrev(stableN);

    int pwm = std::clamp(baseDutyRaw, 0, 255);
    applyDuty(pwm);

    // CSV log (optional): tIndex,tempTrunc,pwm
    // （若你要把 step log 寫檔，可在 config 增加路徑；這裡僅示範記憶體留樣）
    int i = 0;
    bool jumped = false;

    // Cache setpoint truncated (although not used for the steady rule here,
    // we keep it available for potential prints or diagnostics).
    const double spTrunc = numeric::truncateDecimals(
        cfg.temp.setpoint, cfg.basic.truncateDecimals);

    while (i < cfg.basic.maxIterations)
    {
        timeutil::sleepSeconds(poll);

        double temp = sysfs::readTempC(cfg.temp.inputPath);
        temp = numeric::truncateDecimals(temp, cfg.basic.truncateDecimals);

        out.samples.emplace_back(i, temp, pwm);

        // Feed detector; we pass spTrunc but will only query
        // isSteadyAtPrevious() (the detector internally tracks both criteria).
        ssPrev.push(temp, spTrunc);

        if (!jumped)
        {
            // Wait until temperature equals previous for stableN samples.
            if (ssPrev.isSteadyAtPrevious())
            {
                // Perform the step once pre-step steady is achieved.
                pwm = std::clamp(baseDutyRaw + e.stepDuty, 0, 255);
                applyDuty(pwm);

                // Reset detector for post-step settling, still using
                // prev-equals rule.
                ssPrev.reset();
                jumped = true;
            }
        }
        else
        {
            // After the step, stop when it settles again by the same
            // prev-equals rule.
            if (ssPrev.isSteadyAtPrevious())
            {
                break;
            }
        }

        i++;
    }

    out.startDuty = baseDutyRaw;
    if (!out.samples.empty())
    {
        out.endDuty = std::get<2>(out.samples.back());
    }
    else
    {
        out.endDuty = pwm; // fallback
    }

    return out;
}

} // namespace autotune::exp
