#include "buildjson.hpp"

#include "../core/sensorinfo.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace j = nlohmann;

namespace autotune
{

static int read_int(const j::json& obj, const char* key, int def = 0)
{
    auto it = obj.find(key);
    return (it == obj.end()) ? def : it->get<int>();
}

static double read_double(const j::json& obj, const char* key, double def = 0.0)
{
    auto it = obj.find(key);
    return (it == obj.end()) ? def : it->get<double>();
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

    // ===== basic settings =====
    if (!root.contains("basic settings") ||
        !root["basic settings"].is_array() || root["basic settings"].empty())
    {
        throw std::runtime_error("Missing basic settings");
    }
    const auto& basic = root["basic settings"].at(0);
    out.basic.pollIntervalSec = read_int(basic, "pollInterval", 1);
    out.basic.truncateDecimals = read_int(basic, "truncatedecimals", 0);
    out.basic.maxIterations = read_int(basic, "maxiterations", 20000);

    out.basic.steadySlopeThresholdPerSec =
        read_double(basic, "steadyslope", out.basic.steadySlopeThresholdPerSec);
    out.basic.steadyRmseThreshold =
        read_double(basic, "steadyrmse", out.basic.steadyRmseThreshold);
    out.basic.steadyWindow =
        read_int(basic, "steadywindow", out.basic.steadyWindow);

    out.basic.steadySetpointBand =
        read_double(basic, "steadysetpointband", 0.0);

    // Optional sensorinfo path
    out.basic.sensorInfoPath = basic.value("sensorinfopath", std::string{});
    const std::string defaultInfo =
        "/etc/phosphor-pid-autotune/sensorinfo.json";
    sensorinfo::loadFromFile(out.basic.sensorInfoPath.empty()
                                 ? defaultInfo
                                 : out.basic.sensorInfoPath);

    // ===== NEW style: "fansensors" / "tempsensors" =====
    bool parsedNew = false;

    if (root.contains("fansensors") && root["fansensors"].is_array())
    {
        for (const auto& s : root["fansensors"])
        {
            FanChannel f{};
            f.name = s.value("Name", "FAN");
            f.input = s.value("input", "");
            f.minDuty = s.value("minduty", 0);
            f.maxDuty = s.value("maxduty", 255);
            out.fans.push_back(f);
        }
        parsedNew = true;
    }

    if (root.contains("tempsensors") && root["tempsensors"].is_array() &&
        !root["tempsensors"].empty())
    {
        const auto& ts = root["tempsensors"].at(0); // exactly one temp
        out.temp.name = ts.value("Name", "CPU_TEMP");
        out.temp.input = ts.value("input", "");
        out.temp.setpoint = ts.value("setpoint", 70.0);
        out.temp.type = "temp";

        out.temp.sensorType = ts.value("sensortype", std::string{});
        out.temp.pollIntervalSec = ts.value("pollInterval", 0);

        // Lookup sensor info from sensorType
        if (!out.temp.sensorType.empty())
        {
            if (auto ti = sensorinfo::lookupTempInfo(out.temp.sensorType))
            {
                out.temp.qStepC = ti->qStepC;
                out.temp.accuracyC = ti->accuracyC;
                out.temp.bits = ti->bits;
                out.temp.tconvMs = ti->tconvMs;
            }
        }
        parsedNew = true;
    }

    // ===== Legacy fallback: "sensors" mixed array =====
    if (!parsedNew)
    {
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
                // Derive input from Name if not provided
                out.temp.input = s.value("input", out.temp.name);
                out.temp.setpoint = s.value("setpoint", 70.0);
                out.temp.type = "temp";

                out.temp.sensorType = s.value("sensortype", std::string{});
                out.temp.pollIntervalSec = s.value("pollInterval", 0);

                if (s.contains("qstepc"))
                    out.temp.qStepC = s.value("qstepc", out.temp.qStepC);
                if (s.contains("accuracyc"))
                    out.temp.accuracyC =
                        s.value("accuracyc", out.temp.accuracyC);

                if (!out.temp.sensorType.empty())
                {
                    if (auto ti =
                            sensorinfo::lookupTempInfo(out.temp.sensorType))
                    {
                        if (!s.contains("qstepc"))
                            out.temp.qStepC = ti->qStepC;
                        if (!s.contains("accuracyc"))
                            out.temp.accuracyC = ti->accuracyC;
                        out.temp.bits = ti->bits;
                        out.temp.tconvMs = ti->tconvMs;
                    }
                }
            }
            else if (type == "fan")
            {
                FanChannel f{};
                f.name = s.value("Name", "FAN");
                f.input = s.value("input", f.name); // derive from Name
                f.minDuty = s.value("minduty", 0);
                f.maxDuty = s.value("maxduty", 255);
                out.fans.push_back(f);
            }
        }
    }

    // ===== experiments =====
    if (root.contains("experiment") && root["experiment"].is_array())
    {
        for (const auto& e : root["experiment"])
        {
            const std::string type = e.value("type", "");
            if (type == "baseduty")
            {
                BaseDutyExperimentCfg cfg{};
                cfg.logPath = e.value("basedutylog", "");
                cfg.stepOutsideTol = e.value("stepoutsidetol", 10);
                cfg.stepInsideTol = e.value("stepinsidetol", 1);
                out.baseDuty = cfg;
            }
            else if (type == "steptrigger")
            {
                StepTriggerExperimentCfg cfg{};
                cfg.logPath = e.value("stepdutylog", "");
                cfg.stepDuty = e.value("stepduty", 10);
                out.stepTrigger = cfg;
            }
            else if (type == "noise")
            {
                NoiseExperimentCfg cfg{};
                cfg.logPath = e.value("noiselog", "");
                cfg.sampleCount = e.value("samplecount", 100);
                cfg.pollInterval = e.value("pollinterval", 1);
                out.noiseProfile = cfg;
            }
        }
    }

    // ===== process models =====
    if (root.contains("process models") && root["process models"].is_array())
    {
        for (const auto& p : root["process models"])
        {
            const std::string type = p.value("type", "");
            if (type == "fopdt")
            {
                ProcessModelCfg cfg{};
                cfg.logPath = p.value("fopdtlog", "");
                if (p.contains("epsilonfactor"))
                {
                    if (p["epsilonfactor"].is_array())
                    {
                        for (const auto& ef : p["epsilonfactor"])
                            cfg.epsilonFactors.push_back(ef.get<double>());
                    }
                    else
                    {
                        cfg.epsilonFactors.push_back(
                            p.value("epsilonfactor", 1.0));
                    }
                }
                else
                {
                    cfg.epsilonFactors = {1.0};
                }
                out.fopdt = cfg;
            }
        }
    }

    // ===== tuning methods =====
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
                cfg.type = "imc";
                out.imc = cfg;
            }
        }
    }

    // ===== validation =====
    if (out.fans.empty() || out.temp.input.empty())
    {
        throw std::runtime_error(
            "Invalid sensors: require at least one fan and one temp sensor (with 'input').");
    }

    return out;
}

} // namespace autotune
