#include "dbusconfiguration.hpp"

#include "../core/sensorinfo.hpp"
#include "constants.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>

#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace autotune::dbuscfg
{

using Variant =
    std::variant<bool, int64_t, double, std::string, std::vector<std::string>,
                 std::vector<int64_t>, std::vector<double>>;
using InterfaceMap = std::map<std::string, std::map<std::string, Variant>>;
using ManagedObject = std::map<sdbusplus::message::object_path, InterfaceMap>;

static bool getSubTree(
    sdbusplus::bus_t& bus,
    std::map<std::string, std::map<std::string, std::vector<std::string>>>& out)
{
    auto m =
        bus.new_method_call(dbusconst::kMapperService, dbusconst::kMapperPath,
                            dbusconst::kMapperIface, "GetSubTree");
    // Search all paths for any of our config interfaces.
    std::vector<const char*> ifaces = {
        dbusconst::kCfgIfaceBasic, dbusconst::kCfgIfaceSensor,
        dbusconst::kCfgIfaceExperiment, dbusconst::kCfgIfaceProcModel,
        dbusconst::kCfgIfaceTuning};
    m.append("/", 0, ifaces);
    try
    {
        auto reply = bus.call(m);
        reply.read(out);
        return true;
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "[autotune] ObjectMapper.GetSubTree failed: " << e.what()
                  << "\n";
        return false;
    }
}

std::optional<autotune::Config> loadConfigFromEntityManager()
{
    sdbusplus::bus_t bus = sdbusplus::bus::new_default();

    // Load sensorinfo DB once (default path).
    sensorinfo::loadFromFile("/etc/phosphor-pid-autotune/sensorinfo.json");

    std::map<std::string, std::map<std::string, std::vector<std::string>>> tree;
    if (!getSubTree(bus, tree) || tree.empty())
    {
        return std::nullopt;
    }

    ManagedObject objects;

    for (const auto& [path, owners] : tree)
    {
        for (const auto& [service, ifaces] : owners)
        {
            auto m = bus.new_method_call(service.c_str(), path.c_str(),
                                         dbusconst::kObjectManagerIface,
                                         "GetManagedObjects");
            try
            {
                auto reply = bus.call(m);
                ManagedObject mo;
                reply.read(mo);
                objects.insert(mo.begin(), mo.end());
            }
            catch (const sdbusplus::exception_t& e)
            {
                std::cerr << "[autotune] GetManagedObjects failed for "
                          << service << ": " << e.what() << "\n";
            }
        }
    }

    if (objects.empty())
    {
        return std::nullopt;
    }

    autotune::Config cfg{};

    // Parse objects
    for (const auto& [objPath, ifmap] : objects)
    {
        if (ifmap.count(dbusconst::kCfgIfaceBasic))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceBasic);
            if (m.count("pollInterval"))
                cfg.basic.pollIntervalSec =
                    static_cast<int>(std::get<double>(m.at("pollInterval")));
            if (m.count("truncatedecimals"))
                cfg.basic.truncateDecimals = static_cast<int>(
                    std::get<double>(m.at("truncatedecimals")));
            if (m.count("maxiterations"))
                cfg.basic.maxIterations =
                    static_cast<int>(std::get<double>(m.at("maxiterations")));
            if (m.count("steadyslope"))
                cfg.basic.steadySlopeThresholdPerSec =
                    std::get<double>(m.at("steadyslope"));
            if (m.count("steadyrmse"))
                cfg.basic.steadyRmseThreshold =
                    std::get<double>(m.at("steadyrmse"));
            if (m.count("steadywindow"))
                cfg.basic.steadyWindow =
                    static_cast<int>(std::get<double>(m.at("steadywindow")));
            if (m.count("steadysetpointband"))
                cfg.basic.steadySetpointBand =
                    std::get<double>(m.at("steadysetpointband"));
        }

        if (ifmap.count(dbusconst::kCfgIfaceSensor))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceSensor);


            // Infer type if missing: "sensortype" -> temp, "minduty" -> fan
            std::string type;
            if (m.count("type"))
            {
                type = std::get<std::string>(m.at("type"));
            }
            else
            {
                if (m.count("sensortype") || m.count("setpoint"))
                    type = "temp";
                else if (m.count("minduty"))
                    type = "fan";
            }

            if (type == "temp")
            {
                cfg.temp.name = std::get<std::string>(m.at("Name"));
                // New field "input" (D-Bus key). Fallback: derive from Name.
                if (m.count("input"))
                    cfg.temp.input = std::get<std::string>(m.at("input"));
                else
                    cfg.temp.input = cfg.temp.name;

                cfg.temp.setpoint = std::get<double>(m.at("setpoint"));
                cfg.temp.type = "temp";

                if (m.count("sensortype"))
                    cfg.temp.sensorType =
                        std::get<std::string>(m.at("sensortype"));

                if (m.count("pollInterval"))
                    cfg.temp.pollIntervalSec =
                        static_cast<int>(std::get<double>(m.at("pollInterval")));

                // Lookup sensor info.
                if (!cfg.temp.sensorType.empty())
                {
                    if (auto ti =
                            sensorinfo::lookupTempInfo(cfg.temp.sensorType))
                    {
                        cfg.temp.qStepC = ti->qStepC;
                        cfg.temp.accuracyC = ti->accuracyC;
                        cfg.temp.bits = ti->bits;
                        cfg.temp.tconvMs = ti->tconvMs;
                    }
                }
            }
            else if (type == "fan")
            {
                autotune::FanChannel fan{};
                fan.name = std::get<std::string>(m.at("Name"));
                if (m.count("input"))
                    fan.input = std::get<std::string>(m.at("input"));
                else
                    fan.input = fan.name;

                if (m.count("minduty"))
                    fan.minDuty =
                        static_cast<int>(std::get<double>(m.at("minduty")));
                if (m.count("maxduty"))
                    fan.maxDuty =
                        static_cast<int>(std::get<double>(m.at("maxduty")));
                cfg.fans.push_back(fan);
            }
        }

        if (ifmap.count(dbusconst::kCfgIfaceExperiment))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceExperiment);
            const std::string etype = std::get<std::string>(m.at("type"));
            if (etype == "baseduty")
            {
                autotune::BaseDutyExperimentCfg e{};
                if (m.count("basedutylog"))
                    e.logPath = std::get<std::string>(m.at("basedutylog"));
                if (m.count("stepoutsidetol"))
                    e.stepOutsideTol = static_cast<int>(
                        std::get<double>(m.at("stepoutsidetol")));
                if (m.count("stepinsidetol"))
                    e.stepInsideTol = static_cast<int>(
                        std::get<double>(m.at("stepinsidetol")));
                cfg.baseDuty = e;
            }
            else if (etype == "steptrigger")
            {
                autotune::StepTriggerExperimentCfg e{};
                if (m.count("stepdutylog"))
                    e.logPath = std::get<std::string>(m.at("stepdutylog"));
                if (m.count("stepduty"))
                    e.stepDuty =
                        static_cast<int>(std::get<double>(m.at("stepduty")));
                cfg.stepTrigger = e;
            }
            else if (etype == "noise")
            {
                autotune::NoiseExperimentCfg e{};
                if (m.count("noiselog"))
                    e.logPath = std::get<std::string>(m.at("noiselog"));
                if (m.count("samplecount"))
                {
                    const auto& var = m.at("samplecount");
                    if (std::holds_alternative<double>(var))
                        e.sampleCount = static_cast<int>(std::get<double>(var));
                    else if (std::holds_alternative<int64_t>(var))
                        e.sampleCount = static_cast<int>(std::get<int64_t>(var));
                }
                if (m.count("pollinterval"))
                {
                    const auto& var = m.at("pollinterval");
                    if (std::holds_alternative<double>(var))
                        e.pollInterval = static_cast<int>(std::get<double>(var));
                    else if (std::holds_alternative<int64_t>(var))
                        e.pollInterval = static_cast<int>(std::get<int64_t>(var));
                }
                cfg.noiseProfile = e;
            }
        }

        if (ifmap.count(dbusconst::kCfgIfaceProcModel))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceProcModel);
            autotune::ProcessModelCfg p{};
            if (m.count("fopdtlog"))
                p.logPath = std::get<std::string>(m.at("fopdtlog"));
            if (m.count("epsilonfactor"))
            {
                const auto& v =
                    std::get<std::vector<double>>(m.at("epsilonfactor"));
                p.epsilonFactors = v;
            }
            cfg.fopdt = p;
        }

        if (ifmap.count(dbusconst::kCfgIfaceTuning))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceTuning);
            autotune::TuningMethodCfg t{};
            if (m.count("imcpidlog"))
                t.logPath = std::get<std::string>(m.at("imcpidlog"));
            t.type = "imc";
            cfg.imc = t;
        }
    }

    // Basic validation
    if (cfg.fans.empty() || cfg.temp.input.empty())
    {
        return std::nullopt;
    }

    return cfg;
}

} // namespace autotune::dbuscfg
