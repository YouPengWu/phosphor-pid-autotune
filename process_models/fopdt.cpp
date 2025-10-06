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

    const int t0 = std::get<0>(sr.samples.front());
    const double y0 = std::get<1>(sr.samples.front());
    const int u0 = std::get<2>(sr.samples.front());
    const int u1 = std::get<2>(sr.samples.back());

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

    // Gain K: delta y / delta u (both in their native units).
    // Input u is PWM raw 0..255; convert to "percent" for more intuitive K.
    const double duPct = (du / 255.0) * 100.0;
    const double K = dy / duPct;

    // Normalize f(t) = (y - y0) / (yss - y0)
    std::vector<std::pair<int, double>> f;
    f.reserve(sr.samples.size());
    for (const auto& s : sr.samples)
    {
        int t = std::get<0>(s) - t0;
        double y = std::get<1>(s);
        double val = (y - y0) / (yss - y0);
        f.emplace_back(t, val);
    }

    auto find_time_for = [&](double p) -> int {
        // Linear search for first f >= p
        for (size_t i = 0; i < f.size(); ++i)
        {
            if (f[i].second >= p)
            {
                return f[i].first;
            }
        }
        return f.back().first;
    };

    const int t283 = find_time_for(0.283);
    const int t632 = find_time_for(0.632);
    const double T = (static_cast<double>(1.494 * (t632 - t283)));
    const double L = static_cast<double>(t283) - 0.333 * T;

    if (T <= 0.0)
    {
        std::cerr << "[autotune] Invalid T.\n";
        return std::nullopt;
    }

    return FopdtParams{K, T, std::max(0.0, L)};
}

} // namespace autotune::proc
