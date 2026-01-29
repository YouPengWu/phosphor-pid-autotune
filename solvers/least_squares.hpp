#pragma once

#include <cmath>
#include <vector>

namespace autotune::solvers
{

class LeastSquares
{
  public:
    struct Result
    {
        double slope = 0.0;
        double intercept = 0.0;
        bool valid = false;
    };

    /**
     * @brief Perform simple linear regression y = ax + b
     */
    static Result solveLinearRegression(const std::vector<double>& x,
                                        const std::vector<double>& y)
    {
        Result res;
        if (x.size() != y.size() || x.empty())
            return res;

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
            return res;

        res.slope = (n * sumXY - sumX * sumY) / denominator;
        res.intercept = (sumY - res.slope * sumX) / n;
        res.valid = true;

        return res;
    }
};

} // namespace autotune::solvers
