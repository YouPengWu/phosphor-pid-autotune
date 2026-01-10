#include "profile_noise.hpp"

#include "../core/dbus_io.hpp"
#include "../core/logging.hpp"
#include "../core/time_utils.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace autotune::exp
{

autotune::steady::WindowStats runNoiseProfile(const autotune::Config& cfg,
                                              int samples, int intervalSec)
{
    // Safety clamp
    if (samples < 2) samples = 2;
    if (intervalSec < 1) intervalSec = 1;

    std::cout << "[profile_noise] Starting: samples=" << samples
              << ", interval=" << intervalSec << "s\n";

    // Use SteadyStateDetector to track slope/rmse for the ENTIRE window.
    // We pass loose thresholds because we don't care about "isSteady", we just want the stats.
    const double q = cfg.temp.qStepC;
    autotune::steady::SteadyStateDetector detector(samples, intervalSec, 1000.0, 1000.0, q);

    for (int i = 0; i < samples; ++i)
    {
        // Cancel application should happen in main loop handler, but here we run blocking.
        // We'll rely on the fact that if 'wait' is interrupted or if main cancels, we might just finish early.
        // But for now, simple blocking loop.
        
        double val = autotune::dbusio::readTempCByInput(cfg.temp.input);
        detector.push(val);

        if (i < samples - 1)
        {
            autotune::timeutil::sleepSeconds(intervalSec);
        }
    }

    auto stats = detector.stats();

    std::cout << "[profile_noise] Done.\n"
              << "  Slope: " << stats.slope << " deg/s\n"
              << "  RMSE:  " << stats.rmse << " deg\n"
              << "  Mean:  " << stats.mean << " deg\n";
    
    // Log to file if configured
    if (cfg.noiseProfile && !cfg.noiseProfile->logPath.empty())
    {
        try {
            auto parent = std::filesystem::path(cfg.noiseProfile->logPath).parent_path();
            if (!parent.empty())
                std::filesystem::create_directories(parent);
            
            std::ofstream ofs(cfg.noiseProfile->logPath, std::ios::out | std::ios::trunc);
            ofs << "Slope=" << stats.slope << "\n";
            ofs << "RMSE=" << stats.rmse << "\n";
            ofs << "Mean=" << stats.mean << "\n";
            ofs << "Samples=" << samples << "\n";
            ofs << "Interval=" << intervalSec << "\n";
            ofs.flush();
            std::cout << "[profile_noise] Wrote log to " << cfg.noiseProfile->logPath << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[profile_noise] Write log failed: " << e.what() << "\n";
        }
    }

    return stats;
}

} // namespace autotune::exp
