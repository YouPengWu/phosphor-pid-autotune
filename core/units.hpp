#pragma once

#include <string>

namespace autotune::units
{

inline std::string celsius(double t)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2fC", t);
    return std::string(buf);
}

} // namespace autotune::units
