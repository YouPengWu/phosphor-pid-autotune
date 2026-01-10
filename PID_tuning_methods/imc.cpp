#include "imc.hpp"

#include <cmath>
#include <map>

namespace autotune::tuning
{

std::vector<ImcResult> imcFromFopdt(
    const autotune::proc::FopdtParams& p,
    const std::vector<double>& epsilonFactors)
{
    std::vector<ImcResult> out;
    out.reserve(epsilonFactors.size() * 3);

    for (double eps : epsilonFactors)
    {
        double ratio = 0.0;
        if (std::abs(p.theta) > 1e-9)
            ratio = eps / p.theta;
        else
            ratio = 1000.0; // Large ratio if no deadtime

        // 1. PID (Rivera 1986 Table II Row 1)
        {
            double Kc = (2.0 * p.tau + p.theta) / (p.k * (2.0 * eps + p.theta));
            double TauI = p.tau + 0.5 * p.theta;
            double TauD = (p.tau * p.theta) / (2.0 * p.tau + p.theta);
            
            PidGains g{};
            g.Kp = Kc;
            g.Ki = (std::abs(TauI) > 1e-9) ? (Kc / TauI) : 0.0;
            g.Kd = Kc * TauD;
            
            out.push_back({eps, ratio, "PID", g});
        }



        // 3. Improved PI (Rivera 1986 Table II Row 3)
        {
            double den = p.k * 2.0 * eps;
            double Kc = 0.0;
            if (std::abs(den) > 1e-9)
                Kc = (2.0 * p.tau + p.theta) / den;
            
            double TauI = p.tau + 0.5 * p.theta;

            PidGains g{};
            g.Kp = Kc;
            g.Ki = (std::abs(TauI) > 1e-9) ? (Kc / TauI) : 0.0;
            g.Kd = 0.0;

            out.push_back({eps, ratio, "Improved PI", g});
        }
    }

    return out;
}

} // namespace autotune::tuning
