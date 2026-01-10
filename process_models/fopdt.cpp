#include "fopdt.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace autotune::proc
{

std::optional<FopdtParams> identifyFOPDT(const autotune::exp::StepResponse& sr,
                                         double setpoint, int truncateDecimals)
{
    if (sr.samples.size() < 8)
    {
        std::cerr << "[autotune] Not enough samples for FOPDT.\n";
        return std::nullopt;
    }

    const double y0 = std::get<1>(sr.samples.front());
    const int u0 = std::get<2>(sr.samples.front());
    const int u1 = std::get<2>(sr.samples.back());

    // Find the timestamp where the step (input change) actually occurred.
    // We scan for the first sample where input differs from u0.
    // The "step" conceptually happened after the previous sample.
    double t0 = std::get<0>(sr.samples.front());
    size_t stepIdx = 0;
    for (size_t i = 0; i < sr.samples.size(); ++i)
    {
        if (std::get<2>(sr.samples[i]) != u0)
        {
            if (i > 0)
                t0 = std::get<0>(sr.samples[i - 1]);
            else
                t0 = std::get<0>(sr.samples[i]); // Should not happen if steady
            stepIdx = i;
            break;
        }
    }

    const double du = static_cast<double>(u1 - u0);
    if (std::abs(du) < 1e-6)
    {
        std::cerr << "[autotune] No step detected.\n";
        return std::nullopt;
    }

    // Find steady average at the tail end (last quarter)
    size_t n = sr.samples.size();
    size_t start = n - std::max<size_t>(4, n / 4);
    double yss = 0.0;
    size_t cnt = 0;
    for (size_t i = start; i < n; ++i)
    {
        yss += std::get<1>(sr.samples[i]);
        cnt++;
    }
    yss /= static_cast<double>(cnt);

    const double dy = yss - y0;
    if (std::abs(dy) < 1e-6)
    {
        std::cerr << "[autotune] No output change.\n";
        return std::nullopt;
    }

    // Gain k: delta y / delta u (both in their native units).
    // Input u is PWM raw 0..255; convert to "percent" for more intuitive k.
    const double duPct = (du / 255.0) * 100.0;
    const double k = dy / duPct;

    // Normalize f(t) = (y - y0) / (yss - y0)
    std::vector<std::pair<double, double>> f;
    f.reserve(sr.samples.size());
    for (const auto& s : sr.samples)
    {
        double t = std::get<0>(s) - t0;
        double y = std::get<1>(s);
        double val = (y - y0) / (yss - y0);
        f.emplace_back(t, val);
    }

    auto find_time_for = [&](double p) -> double {
        // Search for range where f crosses p, then interpolate
        for (size_t i = 0; i < f.size(); ++i)
        {
            if (f[i].second >= p)
            {
                if (i == 0)
                    return static_cast<double>(f[i].first);

                double t_prev = static_cast<double>(f[i - 1].first);
                double y_prev = f[i - 1].second;
                double t_curr = static_cast<double>(f[i].first);
                double y_curr = f[i].second;

                if (std::abs(y_curr - y_prev) < 1e-9)
                    return t_curr;

                double fraction = (p - y_prev) / (y_curr - y_prev);
                return t_prev + fraction * (t_curr - t_prev);
            }
        }
        return static_cast<double>(f.back().first);
    };

    const double t283 = find_time_for(0.283);
    const double t632 = find_time_for(0.632);
    const double tau = 1.494 * (t632 - t283);
    const double theta = t283 - 0.333 * tau;

    if (tau <= 0.0)
    {
        std::cerr << "[autotune] Invalid tau.\n";
        return std::nullopt;
    }

    return FopdtParams{k, tau, std::max(0.0, theta)};
}

} // namespace autotune::proc
