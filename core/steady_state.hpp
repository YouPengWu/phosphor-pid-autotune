#pragma once

#include <deque>
#include <utility>

namespace autotune::steady
{

// Regression window statistics.
struct WindowStats
{
    double slope{0.0};     // °C/s
    double intercept{0.0}; // °C
    double rmse{0.0};      // °C
    double mean{0.0};      // mean(y) in window
    int n{0};              // sample count
};

// Sliding-window steady state detector based on linear regression.
// Steady if |slope| <= slopeThresh and RMSE <= rmseThresh.
// Thresholds are clamped by quantization floors derived from sensor q (°C/LSB).
class SteadyStateDetector
{
  public:
    SteadyStateDetector(int windowSize, double pollIntervalSec,
                        double slopeThreshPerSec, double rmseThresh,
                        double sensorQuantStepC /* °C/LSB */);

    // Feed a new (already truncated) temperature value.
    void push(double value);

    // True if the last window meets steady-state conditions.
    bool isSteady() const;

    // Compute and return current window regression statistics.
    WindowStats stats() const;

    void reset();

  private:
    int window{10};
    double dt{1.0};
    double userSlopeThresh{0.02};
    double userRmseThresh{0.2};
    double qC{0.0625}; // °C/LSB

    long long count{0};
    std::deque<std::pair<double, double>> samples; // (t, y)
};

} // namespace autotune::steady
