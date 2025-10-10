#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace autotune
{

struct BasicSettings
{
    int pollIntervalSec{};  // sampling period (seconds)
    int truncateDecimals{}; // decimal truncation for temperature samples
    int maxIterations{};    // safety bound for loops

    // Steady-state thresholds for regression detector
    double steadySlopeThresholdPerSec{0.02}; // °C/s
    double steadyRmseThreshold{0.2};         // °C
    int steadyWindow{10};                    // samples

    // Optional extra band on top of sensor accuracy (°C). 0 means disabled.
    double steadySetpointBand{0.0};

    // Optional external path for sensor database (q/accuracy/bits...). Empty ->
    // default path.
    std::string sensorInfoPath;
};

struct TempSensor
{
    std::string name;
    std::string inputPath;
    std::string type;       // logical category, e.g., "temp"
    std::string sensorType; // physical type, e.g., "tmp75"
    double setpoint{};      // °C

    // Resolved from sensorType or explicitly overridden:
    double qStepC{0.0625}; // °C/LSB (quantization step)
    double accuracyC{0.5}; // °C typical absolute accuracy
    int bits{};
    int tconvMs{};
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
    // NOTE: 'tol' removed. Band comes from sensor accuracy + quantization
    // floor.
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
    TempSensor temp; // exactly one temp sensor (current design)
    std::vector<FanChannel> fans;

    std::optional<BaseDutyExperimentCfg> baseDuty;
    std::optional<StepTriggerExperimentCfg> stepTrigger;
    std::optional<ProcessModelCfg> fopdt;
    std::optional<TuningMethodCfg> imc;
};

// Load from file (JSON). Throws std::runtime_error on hard schema issues.
Config loadConfigFromJsonFile(const std::string& jsonPath);

} // namespace autotune
