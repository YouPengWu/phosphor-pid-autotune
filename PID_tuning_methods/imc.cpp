#include "imc.hpp"

#include <cmath>
#include <map>

namespace autotune::tuning
{

std::map<double, PidGains> imcFromFopdt(
    const autotune::proc::FopdtParams& p,
    const std::vector<double>& lambdaFactors)
{
    std::map<double, PidGains> out;

    for (double lam : lambdaFactors)
    {
        const double denom = p.K * (lam + p.L);
        PidGains g{};
        g.Kp = p.T / denom;
        g.Ki = 1.0 / (denom * (p.T + 0.5 * p.L));
        g.Kd = (p.T * p.T * p.L) / (denom * (2.0 * p.T + p.L));
        out[lam] = g;
    }

    return out;
}

} // namespace autotune::tuning
