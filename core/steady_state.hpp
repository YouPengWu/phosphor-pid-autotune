#pragma once

#include <deque>
#include <optional>

namespace autotune::steady
{

// Tracks two steady-state definitions:
// 1) Temperature equals setpoint (after truncation) for >= stableCount samples.
// 2) Temperature equals previous value for >= stableCount samples.
class SteadyStateDetector
{
  public:
    explicit SteadyStateDetector(int stableCount) : required(stableCount) {}

    // Feed a new sample (already truncated).
    void push(double value, double setpoint)
    {
        // Criterion 1: equals setpoint.
        if (value == setpoint)
        {
            sameAsSetpointCount++;
        }
        else
        {
            sameAsSetpointCount = 0;
        }

        // Criterion 2: equals last.
        if (lastSample && *lastSample == value)
        {
            sameAsPrevCount++;
        }
        else
        {
            sameAsPrevCount = 1; // current counts as first
        }

        lastSample = value;
    }

    // True if criterion (1) is satisfied.
    bool isSteadyAtSetpoint() const
    {
        return sameAsSetpointCount >= required;
    }

    // True if criterion (2) is satisfied.
    bool isSteadyAtPrevious() const
    {
        return sameAsPrevCount >= required;
    }

    void reset()
    {
        sameAsSetpointCount = 0;
        sameAsPrevCount = 0;
        lastSample.reset();
    }

  private:
    int required{1};
    int sameAsSetpointCount{0};
    int sameAsPrevCount{0};
    std::optional<double> lastSample;
};

} // namespace autotune::steady
