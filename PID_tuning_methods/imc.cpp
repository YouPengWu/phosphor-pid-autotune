#include "imc.hpp"
#include <cmath>

namespace autotune::tuning
{

std::vector<IMCEntry> calculateIMC(const process_models::FOPDTParameters& modelParams,
                                   const std::vector<double>& epsilonOverTheta)
{
    std::vector<IMCEntry> results;
    
    if (std::abs(modelParams.k) < 1e-9) return results;

    std::vector<double> ratioList = epsilonOverTheta;
    if (ratioList.empty())
    {
        ratioList = {0.5, 0.8, 1.0, 2.0};
    }

    for (double currentRatio : ratioList)
    {
        // currentRatio is epsilon / theta => epsilon = theta * currentRatio
        // If theta is very small (avoid zero), use a default or small epsilon?
        // Actually if theta is zero, FOPDT is weird. But let's assume valid theta.
        double theta = (modelParams.theta < 1e-6) ? 1e-6 : modelParams.theta;
        double epsilon = theta * currentRatio;
        
        double epsilonThetaRatio = (modelParams.theta > 1e-6) ? (epsilon / modelParams.theta) : 999.0;

        // 1. PID Controller
        double numerator = 2.0 * modelParams.tau + modelParams.theta;
        double denominator = modelParams.k * (2.0 * epsilon + modelParams.theta);
        
        double Kc = numerator / denominator;
        double TauI = modelParams.tau + modelParams.theta / 2.0;
        double TauD = (modelParams.tau * modelParams.theta) / (2.0 * modelParams.tau + modelParams.theta);
        
        IMCEntry pidEntry;
        pidEntry.epsilon = epsilon;
        pidEntry.ratio = epsilonThetaRatio;
        pidEntry.type = "PID";
        pidEntry.kp = Kc;
        pidEntry.ki = (std::abs(TauI) > 1e-9) ? (Kc / TauI) : 0;
        pidEntry.kd = Kc * TauD;
        results.push_back(pidEntry);
        
        // 2. Improved PI Controller
        Kc = (2.0 * modelParams.tau + modelParams.theta) / (2.0 * modelParams.k * epsilon);
        TauI = modelParams.tau + modelParams.theta / 2.0;
        
        IMCEntry piEntry;
        piEntry.epsilon = epsilon;
        piEntry.ratio = epsilonThetaRatio;
        piEntry.type = "Improved PI";
        piEntry.kp = Kc;
        piEntry.ki = (std::abs(TauI) > 1e-9) ? (Kc / TauI) : 0;
        piEntry.kd = 0;
        results.push_back(piEntry);
    }

    return results;
}

} // namespace autotune::tuning
