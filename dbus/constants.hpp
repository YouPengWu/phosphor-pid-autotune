#pragma once

namespace autotune::dbusconst
{

// Our service and object for Enable switch.
inline constexpr const char* kEnableService = "xyz.openbmc_project.PIDAutotune";
inline constexpr const char* kEnablePath = "/xyz/openbmc_project/PIDAutotune";
inline constexpr const char* kEnableIface =
    "xyz.openbmc_project.Object.Enable"; // property: Enabled (bool)

// EntityManager interfaces we consume.
inline constexpr const char* kCfgIfaceBasic =
    "xyz.openbmc_project.Configuration.PIDAutotuneBasic";
inline constexpr const char* kCfgIfaceSensor =
    "xyz.openbmc_project.Configuration.PIDAutotuneSensor";
inline constexpr const char* kCfgIfaceExperiment =
    "xyz.openbmc_project.Configuration.PIDAutotuneExperiment";
inline constexpr const char* kCfgIfaceProcModel =
    "xyz.openbmc_project.Configuration.PIDAutotuneProcessModel";
inline constexpr const char* kCfgIfaceTuning =
    "xyz.openbmc_project.Configuration.PIDAutotuneTuningMethod";

// D-Bus helper well-knowns.
inline constexpr const char* kObjectManagerIface =
    "org.freedesktop.DBus.ObjectManager";
inline constexpr const char* kPropertiesIface =
    "org.freedesktop.DBus.Properties";
inline constexpr const char* kMapperService =
    "xyz.openbmc_project.ObjectMapper";
inline constexpr const char* kMapperPath = "/xyz/openbmc_project/object_mapper";
inline constexpr const char* kMapperIface = "xyz.openbmc_project.ObjectMapper";

} // namespace autotune::dbusconst
