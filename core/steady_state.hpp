#pragma once

#include <cmath>
#include <cstddef>
#include <deque>
#include <optional>
#include <tuple>

namespace autotune::steady
{

// Window statistics and regression results (for debugging/telemetry).
struct WindowStats
{
    int n{0};
    double mean{0.0};  // mean(y)
    double slope{0.0}; // b (°C/s)
    double rmse{0.0};  // sqrt(mean(e^2))
    double beff{0.0};  // effective slope threshold (>= user floor)
    double reff{0.0};  // effective rmse threshold (>= sensor floor)
};

// Linear regression + RMSE based steady-state detector.
// It is quantization-aware via qStepC (°C/LSB).
class SteadyStateDetector
{
  public:
    // dtSec: sampling interval (seconds)
    // userSlopeThr: desired slope threshold in °C/s
    // userRmseThr : desired rmse threshold in °C
    // qStepC      : sensor quantization step in °C/LSB (for floors)
    SteadyStateDetector(int window, double dtSec, double userSlopeThr,
                        double userRmseThr, double qStepC) :
        N(window), dt(dtSec), bUser(std::abs(userSlopeThr)),
        eUser(std::abs(userRmseThr)), q(qStepC)
    {
        if (N < 2)
            N = 2;
        if (dt <= 0.0)
            dt = 1.0;
        recomputeFloors();
    }

    // Push a new temperature sample (already truncated if caller desires).
    void push(double y)
    {
        buf.push_back(y);
        // FIX: sliding window must drop the OLDEST element, not the newest one.
        while (static_cast<int>(buf.size()) > N)
            buf.pop_front();

        if (static_cast<int>(buf.size()) >= 2)
            computeStats();
        else
            lastStats = WindowStats{};
    }

    // True if window is full (>=N), |slope| <= beff and rmse <= reff.
    bool isSteady() const
    {
        return lastStats.has_value() &&
               lastStats->n >= N &&
               std::abs(lastStats->slope) <= lastStats->beff &&
               lastStats->rmse <= lastStats->reff;
    }

    // Current window statistics.
    WindowStats stats() const
    {
        return lastStats.value_or(WindowStats{});
    }

    void reset()
    {
        buf.clear();
        lastStats.reset();
    }

  private:
    int N;
    double dt;
    double bUser;
    double eUser;
    double q;         // °C/LSB

    double bMin{0.0}; // floor slope due to quantization
    double eMin{0.0}; // floor rmse due to quantization

    std::deque<double> buf;
    std::optional<WindowStats> lastStats;

    void recomputeFloors()
    {
        // Quantization floors:
        // sigma_q = q / sqrt(12),  b_min = sigma_q / dt
        eMin = q / std::sqrt(12.0);
        bMin = eMin / dt;
    }

    void computeStats()
    {
        const int n = static_cast<int>(buf.size());
        // t_i = 0, dt, 2dt, ... (n-1)dt
        // Use numerically stable single-pass accumulators.
        double sumt = 0.0, sumy = 0.0, sumtt = 0.0, sumty = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double ti = static_cast<double>(i) * dt;
            const double yi = buf[i];
            sumt += ti;
            sumy += yi;
            sumtt += ti * ti;
            sumty += ti * yi;
        }
        const double tbar = sumt / n;
        const double ybar = sumy / n;

        const double Sxx = sumtt - n * tbar * tbar;
        const double Sxy = sumty - n * tbar * ybar;

        double b = 0.0;
        if (Sxx > 0.0)
            b = Sxy / Sxx;

        double sse = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double ti = static_cast<double>(i) * dt;
            const double yi = buf[i];
            const double ei = yi - (ybar + b * (ti - tbar));
            sse += ei * ei;
        }
        const double rmse = std::sqrt(sse / n);

        WindowStats ws;
        ws.n = n;
        ws.mean = ybar;
        ws.slope = b;
        ws.rmse = rmse;
        ws.beff = std::max(bUser, bMin);
        ws.reff = std::max(eUser, eMin);
        lastStats = ws;
    }
};

} // namespace autotune::steady

