#pragma once

#include "../buildjson/config.hpp"
#include "../plot/plot_data.hpp"

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sdbusplus/bus.hpp>
#include <chrono>

namespace autotune::experiment
{

enum class State
{
    Idle,
    InitialWait,
    trigger,
    AfterTriggerWait,
    Finished
};

struct DataPoint
{
    int64_t n;
    double time;
    double temp;
    double pwm;
    double slope;
    double rmse;
    double mean;
};

class StepTrigger
{
public:
    StepTrigger(sdbusplus::bus_t& bus, const std::string& objectPath, // Match definition
                const config::BasicSetting& basic,
                const config::ExperimentConfig& exp,
                const config::ModelConfig& model);

    void tick();
    void setEnabled(bool enabled);
    bool getEnabled() const { return enabled; }

private:
    void start();
    void stop();
    void iteration();
    void finishExperiment();
    void runAnalysis();
    
    // Sub-analysis functions
    void runNoiseAnalysis(const std::string& sensorName);
    void runFOPDTAnalysis(const std::string& sensorName);
    void runIMCPIDAnalysis(const std::string& sensorName);

    sdbusplus::bus_t& bus;
    std::string objectPath;
    config::BasicSetting basicCfg;
    config::ExperimentConfig expCfg;
    config::ModelConfig modelCfg;

    bool enabled = false;
    bool running = false;
    State state = State::Idle;
    
    int64_t currentIteration = 0;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    std::chrono::time_point<std::chrono::steady_clock> lastTickTime;
    
    std::vector<DataPoint> history;
    std::vector<DataPoint> fullLog;
    
    std::string logDir;
    std::ofstream logFile;
    
    plot::PlotLogger plotLogger; // Added PlotLogger
};

} // namespace autotune::experiment
