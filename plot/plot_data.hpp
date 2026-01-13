#pragma once

#include <string>
#include <fstream>

namespace autotune::plot
{

class PlotLogger {
public:
    PlotLogger() = default;
    ~PlotLogger();

    void start(const std::string& outputDir, const std::string& sensorName);
    void log(int64_t n, double pwm, double temp);
    void close();

private:
    std::ofstream file;
};

} // namespace autotune::plot
