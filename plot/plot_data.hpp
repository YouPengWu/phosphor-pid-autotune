#pragma once

#include <fstream>
#include <string>

namespace autotune::plot
{

class PlotLogger
{
  public:
    PlotLogger() = default;
    ~PlotLogger();

    void start(const std::string& outputDir, const std::string& sensorName);
    void log(double time, double pwm, double temp);
    void close();

  private:
    std::ofstream file;
};

} // namespace autotune::plot
