#pragma once

#include "../buildjson/config.hpp"

#include <sdbusplus/bus.hpp>

#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

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
    StepTrigger(sdbusplus::bus_t& bus,
                const std::string& objectPath, // Match definition
                const config::BasicSetting& basic,
                const config::ExperimentConfig& exp);

    void tick();
    void setEnabled(bool enabled);
    bool getEnabled() const
    {
        return enabled;
    }

  private:
    void start();
    void stop();
    void iteration();
    void finishExperiment();
    void runAnalysis();

    // Sub-analysis functions
    void runNoiseAnalysis(const std::string& sensorName);
    void runFOPDTAnalysis(const std::string& sensorName);

    struct AnalysisData
    {
        std::vector<double> times;
        std::vector<double> temps;
        double stepTime;
        double startMean;
        double endMean;
    };
    AnalysisData prepareAnalysisData();

    sdbusplus::bus_t& bus;
    std::string objectPath;
    config::BasicSetting basicCfg;
    config::ExperimentConfig expCfg;

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
};

} // namespace autotune::experiment
