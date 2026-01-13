#pragma once

#include "../process_models/fopdt.hpp"
#include <string>
#include <vector>

namespace autotune::tuning
{

struct IMCEntry
{
    double epsilon;
    double ratio;
    std::string type;
    double kp;
    double ki;
    double kd;
};

std::vector<IMCEntry> calculateIMC(const process_models::FOPDTParameters& params,
                                   const std::vector<double>& tauOverEpsilon);

} // namespace autotune::tuning
