#include "buildjson.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace j = nlohmann;

namespace autotune
{

static int read_int(const j::json& obj, const char* key, int def = 0)
{
    auto it = obj.find(key);
    if (it == obj.end())
    {
        return def;
    }
    return it->get<int>();
}

static double read_double(const j::json& obj, const char* key, double def = 0.0)
{
    auto it = obj.find(key);
    if (it == obj.end())
    {
        return def;
    }
    return it->get<double>();
}

Config loadConfigFromJsonFile(const std::string& jsonPath)
{
    std::ifstream ifs(jsonPath);
    if (!ifs.good())
    {
        throw std::runtime_error("Cannot open config file: " + jsonPath);
    }

    j::json root = j::json::parse(ifs);

    Config out{};

    // basic settings (array of one object per user spec).
    if (!root.contains("basic settings") ||
        !root["basic settings"].is_array() || root["basic settings"].empty())
    {
        throw std::runtime_error("Missing basic settings");
    }
    const auto& basic = root["basic settings"].at(0);
    out.basic.pollIntervalSec = read_int(basic, "pollInterval", 1);
    out.basic.stableCount = read_int(basic, "stablecount", 3);
    out.basic.truncateDecimals = read_int(basic, "truncatedecimals", 0);
    out.basic.maxIterations = read_int(basic, "maxiterations", 10000);

    // sensors section: one temp and multiple fans
    if (!root.contains("sensors") || !root["sensors"].is_array())
    {
        throw std::runtime_error("Missing sensors");
    }
    for (const auto& s : root["sensors"])
    {
        const std::string type = s.value("type", "");
        if (type == "temp")
        {
            out.temp.name = s.value("Name", "CPU_TEMP");
            out.temp.inputPath = s.value("tempxinputpath", "");
            out.temp.setpoint = s.value("setpoint", 70.0);
        }
        else if (type == "fan")
        {
            FanChannel f{};
            f.name = s.value("Name", "FAN");
            f.pwmPath = s.value("pwmxpath", "");
            f.tachPath = s.value("fanxinputpath", "");
            f.minDuty = s.value("minduty", 0);
            f.maxDuty = s.value("maxduty", 255);
            out.fans.push_back(f);
        }
    }

    // experiments
    if (root.contains("experiment") && root["experiment"].is_array())
    {
        for (const auto& e : root["experiment"])
        {
            const std::string type = e.value("type", "");
            if (type == "baseduty")
            {
                BaseDutyExperimentCfg cfg{};
                cfg.logPath = e.value("basedutylog", "");
                cfg.tol = e.value("tol", 1.0);
                cfg.stepOutsideTol = e.value("stepoutsidetol", 10);
                cfg.stepInsideTol = e.value("stepinsidetol", 1);
                cfg.priority = e.value("priority", 1);
                cfg.enabled = e.value("enable", true);
                out.baseDuty = cfg;
            }
            else if (type == "steptrigger")
            {
                StepTriggerExperimentCfg cfg{};
                cfg.logPath = e.value("stepdutylog", "");
                cfg.stepDuty = e.value("stepduty", 10);
                cfg.priority = e.value("priority", 1);
                cfg.enabled = e.value("enable", true);
                out.stepTrigger = cfg;
            }
        }
    }

    // process models
    if (root.contains("process models") && root["process models"].is_array())
    {
        for (const auto& p : root["process models"])
        {
            const std::string type = p.value("type", "");
            if (type == "fopdt")
            {
                ProcessModelCfg cfg{};
                cfg.logPath = p.value("fopdtlog", "");
                if (p.contains("lambdafactor"))
                {
                    if (p["lambdafactor"].is_array())
                    {
                        for (const auto& lf : p["lambdafactor"])
                        {
                            cfg.lambdaFactors.push_back(lf.get<double>());
                        }
                    }
                    else
                    {
                        cfg.lambdaFactors.push_back(
                            p.value("lambdafactor", 1.0));
                    }
                }
                else
                {
                    cfg.lambdaFactors = {1.0};
                }
                cfg.priority = p.value("priority", 1);
                cfg.enabled = true;
                out.fopdt = cfg;
            }
        }
    }

    // tuning methods
    if (root.contains("PID tuning methods") &&
        root["PID tuning methods"].is_array())
    {
        for (const auto& t : root["PID tuning methods"])
        {
            const std::string type = t.value("type", "");
            if (type == "imc")
            {
                TuningMethodCfg cfg{};
                cfg.logPath = t.value("imcpidlog", "");
                cfg.enabled = t.value("enable", true);
                cfg.type = "imc";
                out.imc = cfg;
            }
        }
    }

    return out;
}

} // namespace autotune
