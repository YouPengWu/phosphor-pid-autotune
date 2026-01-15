#pragma once

#include <string>
#include <vector>
#include <map>

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

struct ModelConfig
{
    std::vector<double> epsilonOverTheta;
    std::string tempSensor;
};

struct Config
{
    BasicSetting basic;
    std::vector<ExperimentConfig> experiments;
    std::map<std::string, ModelConfig> models;
};

Config loadConfig(const std::string& path);

} // namespace autotune::config
