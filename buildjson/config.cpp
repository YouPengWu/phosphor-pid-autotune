#include "config.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

namespace autotune::config
{

using json = nlohmann::json;

// Define from_json for easy parsing
void from_json(const json& j, BasicSetting& p)
{
    j.at("pollinterval").get_to(p.pollInterval);
    j.at("windowsize").get_to(p.windowSize);
    // Handle optional or new fields gracefully if needed,
    // but for now strict matching based on provided JSON
    if (j.contains("plot_sampling_rate"))
    {
        j.at("plot_sampling_rate").get_to(p.plotSamplingRate);
    }
    else
    {
        p.plotSamplingRate = 1; // Default
    }
}

void from_json(const json& j, ExperimentConfig& p)
{
    j.at("initialfansensors").get_to(p.initialFanSensors);
    j.at("initialpwmduty").get_to(p.initialPwmDuty);
    j.at("aftertriggerfansensors").get_to(p.afterTriggerFanSensors);
    j.at("aftertriggerpwmduty").get_to(p.afterTriggerPwmDuty);
    j.at("initialiterations").get_to(p.initialIterations);
    j.at("aftertriggeriterations").get_to(p.afterTriggerIterations);
    j.at("tempsensor").get_to(p.tempSensor);
}

Config loadConfig(const std::string& path)
{
    Config cfg;
    std::ifstream i(path);
    if (!i.is_open())
    {
        std::cerr << "Failed to open config file: " << path << "\n";
        return cfg;
    }

    json j;
    try
    {
        i >> j;

        // Parse BasicSetting (it's an array in the JSON, we take the first one)
        if (j.contains("basicsetting") && j["basicsetting"].is_array() &&
            !j["basicsetting"].empty())
        {
            cfg.basic = j["basicsetting"][0].get<BasicSetting>();
        }

        // Parse Experiments
        if (j.contains("experiment") && j["experiment"].is_array())
        {
            cfg.experiments =
                j["experiment"].get<std::vector<ExperimentConfig>>();
        }
    }
    catch (const json::exception& e)
    {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        // Depending on requirements, might want to throw or return partial
        // config
    }

    return cfg;
}

} // namespace autotune::config
