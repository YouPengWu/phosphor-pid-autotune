#pragma once

#include <map>
#include <string>
#include <vector>

namespace autotune::config
{

struct BasicSetting
{
    int pollInterval;
    int windowSize;
    int plotSamplingRate;
};

struct ExperimentConfig
{
    std::vector<std::string> initialFanSensors;
    double initialPwmDuty;
    std::vector<std::string> afterTriggerFanSensors;
    double afterTriggerPwmDuty;
    int initialIterations;
    int afterTriggerIterations;
    std::string tempSensor;
};

struct Config
{
    BasicSetting basic;
    std::vector<ExperimentConfig> experiments;
};

Config loadConfig(const std::string& path);

} // namespace autotune::config
