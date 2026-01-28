#include "utils.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace autotune::core
{

double calculateSlope(const std::vector<double>& data,
                      const std::vector<double>& time, size_t windowSize)
{
    if (data.size() < windowSize || time.size() < windowSize || windowSize < 2)
        return 0.0;

    size_t start = data.size() - windowSize;
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

    for (size_t i = 0; i < windowSize; ++i)
    {
        double x = time[start + i];
        double y = data[start + i];
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double denominator = windowSize * sumX2 - sumX * sumX;
    return (std::abs(denominator) < 1e-6)
               ? 0.0
               : ((windowSize * sumXY - sumX * sumY) / denominator);
}

double calculateRMSE(const std::vector<double>& data, size_t windowSize)
{
    if (data.size() < windowSize || windowSize == 0)
        return 0.0;

    double mean = calculateMean(data, windowSize);
    double sumSqDiff = 0.0;
    size_t start = data.size() - windowSize;

    for (size_t i = 0; i < windowSize; ++i)
    {
        double diff = data[start + i] - mean;
        sumSqDiff += diff * diff;
    }

    return std::sqrt(sumSqDiff / windowSize);
}

double calculateMean(const std::vector<double>& data, size_t windowSize)
{
    if (data.size() < windowSize || windowSize == 0)
        return 0.0;

    size_t start = data.size() - windowSize;
    double sum = 0.0;
    for (size_t i = 0; i < windowSize; ++i)
        sum += data[start + i];

    return sum / windowSize;
}

int scalePwmToRaw(double dutyCycle)
{
    return static_cast<int>(std::clamp(dutyCycle, 0.0, 100.0) * 255.0 / 100.0);
}

double scaleRawToDuty(int rawPwm)
{
    return static_cast<double>(std::clamp(rawPwm, 0, 255)) * 100.0 / 255.0;
}

double linearInterpolateX(double y, double x1, double y1, double x2, double y2)
{
    if (std::abs(y2 - y1) < 1e-9)
        return x1;
    return x1 + (y - y1) * (x2 - x1) / (y2 - y1);
}

bool calculateLinearRegression(const std::vector<double>& x,
                               const std::vector<double>& y, double& slope,
                               double& intercept)
{
    if (x.size() != y.size() || x.empty())
        return false;

    size_t n = x.size();
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i];
    }

    double denominator = n * sumX2 - sumX * sumX;
    if (std::abs(denominator) < 1e-9)
        return false;

    slope = (n * sumXY - sumX * sumY) / denominator;
    intercept = (sumY - slope * sumX) / n;

    return true;
}

bool solveLeastSquaresFOPDT(
    const std::vector<double>& time, const std::vector<double>& temp,
    double stepTime, double initialTemp, double finalTemp, double initialPwm,
    double stepPwm, double& k, double& tau, double& theta)
{
    // Linear Regression with Inverted Variables (Robust)
    // Model: t = tau * [-ln(1-p)] + theta

    double temperatureChange = finalTemp - initialTemp;
    double initialDuty = scaleRawToDuty(static_cast<int>(initialPwm));
    double stepDuty = scaleRawToDuty(static_cast<int>(stepPwm));
    double dutyChange = stepDuty - initialDuty;

    if (std::abs(dutyChange) < 1e-6)
        return false;

    k = temperatureChange / dutyChange;

    std::vector<double> xLog;
    std::vector<double> yLog;

    for (size_t i = 0; i < time.size(); ++i)
    {
        if (time[i] < stepTime)
            continue;

        double currentTemp = temp[i];
        double yNorm = (currentTemp - initialTemp) / temperatureChange;

        // Threshold 10% to 90%
        if (yNorm > 0.1 && yNorm < 0.9)
        {
            double val = 1.0 - yNorm;
            if (val > 1e-9) // Ensure we can take log
            {
                // X = -ln(1 - yNorm)
                double lx = -std::log(val);
                // Y = t_rel (time - stepTime)
                double ly = time[i] - stepTime;

                xLog.push_back(lx);
                yLog.push_back(ly);
            }
        }
    }

    // Dump regression data to file
    {
        std::ofstream dFile("lsm_debug.csv");
        dFile << "x_ln,y_time\n";
        for (size_t i = 0; i < xLog.size(); ++i)
        {
            dFile << xLog[i] << "," << yLog[i] << "\n";
        }
    }

    double slope = 0.0;
    double intercept = 0.0;

    std::cout << "[DEBUG] LSM: T_init=" << initialTemp << " T_final="
              << finalTemp << " T_change=" << temperatureChange << "\n";
    std::cout << "[DEBUG] LSM: Points collected=" << xLog.size() << "\n";

    if (calculateLinearRegression(xLog, yLog, slope, intercept))
    {
        std::cout << "[DEBUG] LSM: Slope=" << slope
                  << " Intercept=" << intercept << "\n";

        // Slope is directly Tau in this formulation
        if (slope > 1e-9)
        {
            tau = slope;
            theta = intercept;

            if (theta < 0)
                theta = 0;
            return true;
        }
        else
        {
            std::cout << "[DEBUG] LSM: Slope (Tau) must be positive.\n";
        }
    }
    else
    {
        std::cout << "[DEBUG] LSM: Reg failed.\n";
    }
    return false;
}

bool calculateTwoPointFOPDT(
    const std::vector<double>& time, const std::vector<double>& temp,
    double stepTime, double initialTemp, double finalTemp, double initialPwm,
    double stepPwm, double& k, double& tau, double& theta)
{
    // K Calculation
    double temperatureChange = finalTemp - initialTemp;
    double initialDuty = scaleRawToDuty(static_cast<int>(initialPwm));
    double stepDuty = scaleRawToDuty(static_cast<int>(stepPwm));
    double dutyChange = stepDuty - initialDuty;

    if (std::abs(dutyChange) < 1e-6)
        return false;

    k = temperatureChange / dutyChange;

    // Time Constant Calculation
    double temp28 = initialTemp + 0.283 * temperatureChange;
    double temp63 = initialTemp + 0.632 * temperatureChange;

    double time28 = -1.0;
    double time63 = -1.0;

    for (size_t i = 1; i < time.size(); ++i)
    {
        if (time[i] < stepTime)
            continue;

        double t1 = time[i - 1];
        double y1 = temp[i - 1];
        double t2 = time[i];
        double y2 = temp[i];

        if (time28 < 0)
        {
            if ((temperatureChange > 0 && y1 < temp28 && y2 >= temp28) ||
                (temperatureChange < 0 && y1 > temp28 && y2 <= temp28))
            {
                time28 = linearInterpolateX(temp28, t1, y1, t2, y2);
            }
        }

        if (time63 < 0)
        {
            if ((temperatureChange > 0 && y1 < temp63 && y2 >= temp63) ||
                (temperatureChange < 0 && y1 > temp63 && y2 <= temp63))
            {
                time63 = linearInterpolateX(temp63, t1, y1, t2, y2);
            }
        }

        if (time28 > 0 && time63 > 0)
            break;
    }

    if (time28 > 0 && time63 > 0)
    {
        tau = 1.5 * (time63 - time28);
        theta = (time63 - stepTime) - tau;
        if (theta < 0)
            theta = 0;
        return true;
    }

    return false;
}

} // namespace autotune::core
