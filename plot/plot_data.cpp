#include "plot_data.hpp"

#include <filesystem>
#include <iostream>

namespace autotune::plot
{

namespace fs = std::filesystem;

PlotLogger::~PlotLogger()
{
    close();
}

void PlotLogger::start(const std::string& outputDir,
                       const std::string& sensorName)
{
    close(); // Close if already open

    try
    {
        fs::create_directories(outputDir);
    }
    catch (const fs::filesystem_error& e)
    {
        std::cerr << "Error creating plot dir: " << e.what() << "\n";
    }

    std::string filename = outputDir + "/plot_" + sensorName + ".txt";
    file.open(filename, std::ios::out | std::ios::trunc);

    if (file.is_open())
    {
        file << "time pwm temp\n";
        file.flush();
        std::cout << "[Plot] Started log: " << filename << "\n";
    }
    else
    {
        std::cerr << "[Plot] Failed to open: " << filename << "\n";
    }
}

void PlotLogger::log(double time, double pwm, double temp)
{
    if (file.is_open())
    {
        file << time << " " << pwm << " " << temp << "\n";
        file.flush();
    }
}

void PlotLogger::close()
{
    if (file.is_open())
    {
        file.close();
    }
}

} // namespace autotune::plot
