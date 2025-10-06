#include "sysfs_io.hpp"

#include <fstream>
#include <iostream>

namespace autotune::sysfs
{

void writePwmAll(const std::vector<std::string>& pwmPaths, int raw)
{
    for (const auto& p : pwmPaths)
    {
        std::ofstream ofs(p);
        if (!ofs.good())
        {
            std::cerr << "[autotune] Failed to open PWM path: " << p << "\n";
            continue;
        }
        ofs << raw << std::endl;
    }
}

double readTempC(const std::string& tempInputPath)
{
    std::ifstream ifs(tempInputPath);
    if (!ifs.good())
    {
        std::cerr << "[autotune] Failed to read temp path: " << tempInputPath
                  << "\n";
        return 0.0;
    }
    long milli = 0;
    ifs >> milli;
    return static_cast<double>(milli) / 1000.0;
}

int readTach(const std::string& tachInputPath)
{
    std::ifstream ifs(tachInputPath);
    if (!ifs.good())
    {
        return 0;
    }
    int rpm = 0;
    ifs >> rpm;
    return rpm;
}

} // namespace autotune::sysfs
