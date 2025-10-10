#include "steady_state.hpp"

#include <algorithm>
#include <cmath>

namespace autotune::steady
{

SteadyStateDetector::SteadyStateDetector(
    int windowSize, double pollIntervalSec, double slopeThreshPerSec,
    double rmseThresh, double sensorQuantStepC) :
    window(std::max(2, windowSize)),
    dt(pollIntervalSec > 0 ? pollIntervalSec : 1.0),
    userSlopeThresh(std::abs(slopeThreshPerSec)), userRmseThresh(rmseThresh),
    qC(sensorQuantStepC > 0 ? sensorQuantStepC : 0.0625)
{}

void SteadyStateDetector::push(double value)
{
    double t = static_cast<double>(count) * dt;
    if (samples.size() == static_cast<size_t>(window))
        samples.pop_front();
    samples.emplace_back(t, value);
    ++count;
}

WindowStats SteadyStateDetector::stats() const
{
    WindowStats ws;
    if (samples.size() < static_cast<size_t>(window))
    {
        ws.n = static_cast<int>(samples.size());
        return ws;
    }

    const int n = static_cast<int>(samples.size());
    double sumT = 0.0, sumY = 0.0, sumTT = 0.0, sumTY = 0.0;
    for (const auto& [t, y] : samples)
    {
        sumT += t;
        sumY += y;
        sumTT += t * t;
        sumTY += t * y;
    }
    const double denom = (n * sumTT - sumT * sumT);

    double slope = 0.0;
    double intercept = 0.0;
    if (std::abs(denom) > 1e-12)
    {
        slope = (n * sumTY - sumT * sumY) / denom; // °C/s
        intercept = (sumY - slope * sumT) / n;
    }
    else
    {
        slope = 0.0;
        intercept = sumY / n;
    }

    double se = 0.0;
    for (const auto& [t, y] : samples)
    {
        const double yhat = intercept + slope * t;
        const double r = (y - yhat);
        se += r * r;
    }
    const double rmse = std::sqrt(se / n);

    ws.slope = slope;
    ws.intercept = intercept;
    ws.rmse = rmse;
    ws.mean = sumY / n;
    ws.n = n;
    return ws;
}

bool SteadyStateDetector::isSteady() const
{
    auto ws = stats();
    if (ws.n < window)
        return false;

    // Quantization-aware floors
    const double rmseMin = qC / std::sqrt(12.0);
    const double slopeMin = rmseMin / dt;

    const double slopeThresh = std::max(userSlopeThresh, slopeMin);
    const double rmseThresh = std::max(userRmseThresh, rmseMin);

    return (std::abs(ws.slope) <= slopeThresh) && (ws.rmse <= rmseThresh);
}

void SteadyStateDetector::reset()
{
    samples.clear();
    count = 0;
}

} // namespace autotune::steady
