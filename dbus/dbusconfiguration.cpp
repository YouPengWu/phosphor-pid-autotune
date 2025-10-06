#include "dbusconfiguration.hpp"

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
            cfg.basic.pollIntervalSec =
                static_cast<int>(std::get<double>(m.at("pollInterval")));
            cfg.basic.stableCount =
                static_cast<int>(std::get<double>(m.at("stablecount")));
            cfg.basic.truncateDecimals =
                static_cast<int>(std::get<double>(m.at("truncatedecimals")));
            if (m.count("maxiterations"))
            {
                cfg.basic.maxIterations =
                    static_cast<int>(std::get<double>(m.at("maxiterations")));
            }
        }
        if (ifmap.count(dbusconst::kCfgIfaceSensor))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceSensor);
            const std::string type = std::get<std::string>(m.at("type"));
            if (type == "temp")
            {
                cfg.temp.name = std::get<std::string>(m.at("Name"));
                cfg.temp.inputPath =
                    std::get<std::string>(m.at("tempxinputpath"));
                cfg.temp.setpoint = std::get<double>(m.at("setpoint"));
            }
            else if (type == "fan")
            {
                autotune::FanChannel fan{};
                fan.name = std::get<std::string>(m.at("Name"));
                fan.pwmPath = std::get<std::string>(m.at("pwmxpath"));
                fan.tachPath = std::get<std::string>(m.at("fanxinputpath"));
                if (m.count("minduty"))
                {
                    fan.minDuty =
                        static_cast<int>(std::get<double>(m.at("minduty")));
                }
                if (m.count("maxduty"))
                {
                    fan.maxDuty =
                        static_cast<int>(std::get<double>(m.at("maxduty")));
                }
                cfg.fans.push_back(fan);
            }
        }
        if (ifmap.count(dbusconst::kCfgIfaceExperiment))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceExperiment);
            const std::string type = std::get<std::string>(m.at("type"));
            if (type == "baseduty")
            {
                autotune::BaseDutyExperimentCfg e{};
                e.logPath = std::get<std::string>(m.at("basedutylog"));
                e.tol = std::get<double>(m.at("tol"));
                e.stepOutsideTol =
                    static_cast<int>(std::get<double>(m.at("stepoutsidetol")));
                e.stepInsideTol =
                    static_cast<int>(std::get<double>(m.at("stepinsidetol")));
                e.priority =
                    static_cast<int>(std::get<double>(m.at("priority")));
                e.enabled = std::get<bool>(m.at("enable"));
                cfg.baseDuty = e;
            }
            else if (type == "steptrigger")
            {
                autotune::StepTriggerExperimentCfg e{};
                e.logPath = std::get<std::string>(m.at("stepdutylog"));
                e.stepDuty =
                    static_cast<int>(std::get<double>(m.at("stepduty")));
                e.priority =
                    static_cast<int>(std::get<double>(m.at("priority")));
                e.enabled = std::get<bool>(m.at("enable"));
                cfg.stepTrigger = e;
            }
        }
        if (ifmap.count(dbusconst::kCfgIfaceProcModel))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceProcModel);
            autotune::ProcessModelCfg p{};
            p.logPath = std::get<std::string>(m.at("fopdtlog"));
            if (m.count("lambdafactor"))
            {
                const auto& v =
                    std::get<std::vector<double>>(m.at("lambdafactor"));
                p.lambdaFactors = v;
            }
            if (m.count("priority"))
            {
                p.priority =
                    static_cast<int>(std::get<double>(m.at("priority")));
            }
            p.enabled = true;
            cfg.fopdt = p;
        }
        if (ifmap.count(dbusconst::kCfgIfaceTuning))
        {
            const auto& m = ifmap.at(dbusconst::kCfgIfaceTuning);
            autotune::TuningMethodCfg t{};
            t.logPath = std::get<std::string>(m.at("imcpidlog"));
            t.enabled = std::get<bool>(m.at("enable"));
            t.type = "imc";
            cfg.imc = t;
        }
    }

    // Basic validation
    if (cfg.fans.empty() || cfg.temp.inputPath.empty())
    {
        return std::nullopt;
    }

    return cfg;
}

} // namespace autotune::dbuscfg
