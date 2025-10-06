#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace autotune
{

struct BasicSettings
{
    int pollIntervalSec{};
    int stableCount{};
    int truncateDecimals{};
    int maxIterations{};
};

struct TempSensor
{
    std::string name;
    std::string inputPath;
    double setpoint{};
};

struct FanChannel
{
    std::string name;
    std::string pwmPath;
    std::string tachPath;
    int minDuty{}; // 0-255
    int maxDuty{}; // 0-255
};

struct BaseDutyExperimentCfg
{
    std::string logPath;
    double tol{};
    int stepOutsideTol{};
    int stepInsideTol{};
    int priority{};
    bool enabled{};
};

struct StepTriggerExperimentCfg
{
    std::string logPath;
    int stepDuty{}; // 0-255
    int priority{};
    bool enabled{};
};

struct ProcessModelCfg
{
    std::string logPath;
    std::vector<double> lambdaFactors;
    int priority{};
    bool enabled{true};
};

struct TuningMethodCfg
{
    std::string logPath;
    bool enabled{};
    std::string type; // "imc"
};

struct Config
{
    BasicSettings basic;
    TempSensor temp;
    std::vector<FanChannel> fans;
    std::optional<BaseDutyExperimentCfg> baseDuty;
    std::optional<StepTriggerExperimentCfg> stepTrigger;
    std::optional<ProcessModelCfg> fopdt;
    std::optional<TuningMethodCfg> imc;
};

// Load from /etc or our shipped configs (path given). Throws
// std::runtime_error.
Config loadConfigFromJsonFile(const std::string& jsonPath);

} // namespace autotune
