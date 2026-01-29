#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace autotune::solvers
{

/**
 * @brief Nelder-Mead Optimization Solver.
 * Minimizes a cost function of N variables.
 */
class NelderMead
{
  public:
    using CostFunction = std::function<double(const std::vector<double>&)>;

    struct Point
    {
        std::vector<double> params;
        double cost;
    };

    /**
     * @brief Solve the optimization problem.
     * @param initialParams Initial guess for parameters.
     * @param costFunc Function to minimize.
     * @param maxIter Maximum iterations (default 200).
     * @return Optimized parameters.
     */
    static std::vector<double> solve(const std::vector<double>& initialParams,
                                     CostFunction costFunc, int maxIter = 200)
    {
        size_t n = initialParams.size(); // Number of dimensions
        std::vector<Point> simplex(n + 1);

        // 1. Initialize Simplex
        // P0 = initial guess
        simplex[0].params = initialParams;
        simplex[0].cost = costFunc(initialParams);

        // P1..Pn = perturbed P0
        for (size_t i = 1; i <= n; ++i)
        {
            simplex[i].params = initialParams;
            // Perturb i-th dimension by 5% or minimally 0.00025
            if (std::abs(simplex[i].params[i - 1]) > 1e-9)
            {
                simplex[i].params[i - 1] *= 1.05;
            }
            else
            {
                simplex[i].params[i - 1] = 0.00025;
            }
            simplex[i].cost = costFunc(simplex[i].params);
        }

        // Parameters
        const double alpha = 1.0; // Reflection
        const double gamma = 2.0; // Expansion
        const double rho = 0.5;   // Contraction
        const double sigma = 0.5; // Shrink

        for (int iter = 0; iter < maxIter; ++iter)
        {
            // 2. Sort Simplex (Best to Worst)
            std::sort(simplex.begin(), simplex.end(),
                      [](const Point& a, const Point& b) {
                          return a.cost < b.cost;
                      });

            // Best is simplex[0], Worst is simplex[n], Second Worst is
            // simplex[n-1]
            const auto& best = simplex[0];
            const auto& worst = simplex[n];
            const auto& secondWorst = simplex[n - 1];

            // Check for convergence (simple cost variance check)
            double range = std::abs(worst.cost - best.cost);
            if (range < 1e-6)
                break;

            // 3. Centroid (of all points except worst)
            std::vector<double> centroid(n, 0.0);
            for (size_t i = 0; i < n; ++i)
            {
                for (size_t j = 0; j < n; ++j)
                {
                    centroid[j] += simplex[i].params[j];
                }
            }
            for (size_t j = 0; j < n; ++j)
                centroid[j] /= n;

            // 4. Reflection
            std::vector<double> reflected(n);
            for (size_t j = 0; j < n; ++j)
                reflected[j] = centroid[j] +
                               alpha * (centroid[j] - worst.params[j]);

            double reflectedCost = costFunc(reflected);

            if (best.cost <= reflectedCost && reflectedCost < secondWorst.cost)
            {
                // Accept Reflection
                simplex[n].params = reflected;
                simplex[n].cost = reflectedCost;
                continue;
            }

            // 5. Expansion
            if (reflectedCost < best.cost)
            {
                std::vector<double> expanded(n);
                for (size_t j = 0; j < n; ++j)
                    expanded[j] = centroid[j] +
                                  gamma * (reflected[j] - centroid[j]);

                double expandedCost = costFunc(expanded);
                if (expandedCost < reflectedCost)
                {
                    simplex[n].params = expanded;
                    simplex[n].cost = expandedCost;
                }
                else
                {
                    simplex[n].params = reflected;
                    simplex[n].cost = reflectedCost;
                }
                continue;
            }

            // 6. Contraction
            // Here reflectedCost >= secondWorst.cost
            bool isOutside =
                (reflectedCost < worst.cost); // True if better than worst

            std::vector<double> contracted(n);
            double contractedCost = 0.0;

            if (isOutside) // Outside Contraction (using Reflected)
            {
                for (size_t j = 0; j < n; ++j)
                    contracted[j] = centroid[j] +
                                    rho * (reflected[j] - centroid[j]);
                contractedCost = costFunc(contracted);

                if (contractedCost <= reflectedCost)
                {
                    simplex[n].params = contracted;
                    simplex[n].cost = contractedCost;
                    continue;
                }
            }
            else // Inside Contraction (using Worst)
            {
                for (size_t j = 0; j < n; ++j)
                    contracted[j] = centroid[j] +
                                    rho * (worst.params[j] - centroid[j]);
                contractedCost = costFunc(contracted);

                if (contractedCost < worst.cost)
                {
                    simplex[n].params = contracted;
                    simplex[n].cost = contractedCost;
                    continue;
                }
            }

            // 7. Shrink
            for (size_t i = 1; i <= n; ++i)
            {
                for (size_t j = 0; j < n; ++j)
                {
                    simplex[i].params[j] =
                        best.params[j] +
                        sigma * (simplex[i].params[j] - best.params[j]);
                }
                simplex[i].cost = costFunc(simplex[i].params);
            }
        }

        // Final Sort
        std::sort(simplex.begin(), simplex.end(),
                  [](const Point& a, const Point& b) {
                      return a.cost < b.cost;
                  });

        return simplex[0].params;
    }
};

} // namespace autotune::solvers
