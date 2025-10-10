#include "dbus_io.hpp"

#include "../dbus/constants.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace autotune::dbusio
{

static sdbusplus::bus_t& bus()
{
    static sdbusplus::bus_t b = sdbusplus::bus::new_default();
    return b;
}

// Resolve owning service (path, interface) via ObjectMapper.GetObject.
static std::optional<std::string> getService(const std::string& path,
                                             const std::string& iface)
{
    try
    {
        auto m = bus().new_method_call(
            autotune::dbusconst::kMapperService,
            autotune::dbusconst::kMapperPath, autotune::dbusconst::kMapperIface,
            "GetObject");

        std::vector<std::string> ifaces = {iface};
        m.append(path, ifaces);

        std::map<std::string, std::vector<std::string>> owners;
        auto reply = bus().call(m);
        reply.read(owners);

        if (owners.empty())
            return std::nullopt;

        return owners.begin()->first;
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "[autotune] Mapper.GetObject failed for " << path
                  << " iface=" << iface << ": " << e.what() << "\n";
        return std::nullopt;
    }
}

// Properties.Get → double (accepts double/int64/uint64 then casts).
static std::optional<double> getDouble(
    const std::string& path, const std::string& iface, const std::string& prop)
{
    auto svc = getService(path, iface);
    if (!svc)
        return std::nullopt;

    try
    {
        auto m =
            bus().new_method_call(svc->c_str(), path.c_str(),
                                  autotune::dbusconst::kPropertiesIface, "Get");
        m.append(iface, prop);

        using V = std::variant<double, int64_t, uint64_t, bool, std::string>;
        V v;
        auto reply = bus().call(m);
        reply.read(v);

        if (auto pd = std::get_if<double>(&v))
            return *pd;
        if (auto pi = std::get_if<int64_t>(&v))
            return static_cast<double>(*pi);
        if (auto pu = std::get_if<uint64_t>(&v))
            return static_cast<double>(*pu);

        std::cerr << "[autotune] Get " << path << " " << iface << "." << prop
                  << " not numeric\n";
        return std::nullopt;
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "[autotune] Properties.Get failed for " << path << " "
                  << iface << "." << prop << ": " << e.what() << "\n";
        return std::nullopt;
    }
}

static bool setUint64(const std::string& path, const std::string& iface,
                      const std::string& prop, uint64_t val)
{
    auto svc = getService(path, iface);
    if (!svc)
        return false;

    try
    {
        auto m =
            bus().new_method_call(svc->c_str(), path.c_str(),
                                  autotune::dbusconst::kPropertiesIface, "Set");
        std::variant<uint64_t> v = val;
        m.append(iface, prop, v);
        (void)bus().call(m);
        return true;
    }
    catch (const sdbusplus::exception_t& e)
    {
        std::cerr << "[autotune] Properties.Set failed for " << path << " "
                  << iface << "." << prop << ": " << e.what() << "\n";
        return false;
    }
}

double readTempCByInput(const std::string& input)
{
    const std::string path =
        "/xyz/openbmc_project/sensors/temperature/" + input;
    const std::string iface = "xyz.openbmc_project.Sensor.Value";
    const std::string prop = "Value";
    auto v = getDouble(path, iface, prop);
    return v.value_or(0.0);
}

bool writePwmAllByInput(const std::vector<std::string>& inputs, int raw)
{
    const uint64_t u = static_cast<uint64_t>(std::clamp(raw, 0, 255));
    const std::string iface = "xyz.openbmc_project.Control.FanPwm";
    const std::string prop = "Target";

    bool ok = true;
    for (const auto& in : inputs)
    {
        const std::string path = "/xyz/openbmc_project/control/fanpwm/" + in;
        ok = setUint64(path, iface, prop, u) && ok;
    }
    return ok;
}

std::optional<double> readFanPctByInput(const std::string& input)
{
    const std::string path = "/xyz/openbmc_project/sensors/fan_pwm/" + input;
    const std::string iface = "xyz.openbmc_project.Sensor.Value";
    const std::string prop = "Value";
    return getDouble(path, iface, prop);
}

} // namespace autotune::dbusio
