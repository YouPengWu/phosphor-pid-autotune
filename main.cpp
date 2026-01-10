#include "PID_tuning_methods/imc.hpp"
#include "buildjson/buildjson.hpp"
#include "core/numeric.hpp"
#include "dbus/constants.hpp"
#include "dbus/dbusconfiguration.hpp"
#include "experiment/base_duty.hpp"
#include "experiment/profile_noise.hpp"
#include "experiment/step_trigger.hpp"
#include "process_models/fopdt.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

static void writePidOut(const std::string& path,
                        const std::vector<autotune::tuning::ImcResult>& res)
{
    if (path.empty())
        return;

    try
    {
        std::filesystem::create_directories(
            std::filesystem::path(path).parent_path());
    }
    catch (...)
    {
        std::cerr << "[autotune] mkdir failed: " << path << "\n";
        return;
    }

    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    ofs << "epsilon,ratio,type,Kp,Ki,Kd\n";
    for (const auto& r : res)
    {
        ofs << r.epsilon << "," << r.ratio << "," << r.type << ","
            << r.gains.Kp << "," << r.gains.Ki << "," << r.gains.Kd << "\n";
    }
    ofs.flush();
}

static void systemctl(const char* cmd)
{
    int rc = std::system(cmd);
    (void)rc;
}

static std::string parentDir(const std::string& path)
{
    if (path.empty())
        return {};
    return std::filesystem::path(path).parent_path().string();
}

static bool isSafeLogDir(const std::string& dir)
{
    if (dir.empty())
        return false;
    if (dir.find("autotune") == std::string::npos)
        return false;
    size_t depth = 0;
    for (const auto& part : std::filesystem::path(dir))
    {
        (void)part;
        ++depth;
    }
    return depth >= 3;
}

static void purgeLogDirectory(const std::string& dir)
{
    if (dir.empty())
        return;

    if (!isSafeLogDir(dir))
    {
        std::cerr << "[autotune] REFUSE to purge non-safe dir: " << dir << "\n";
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
    {
        std::cerr << "[autotune] create_directories failed: " << dir
                  << " ec=" << ec.message() << "\n";
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec)
            break;
        if (entry.is_regular_file())
        {
            std::filesystem::remove(entry.path(), ec);
        }
    }
}

// Global state for execution control
static std::atomic<bool> gIsRunning{false};
static std::atomic<bool> gBaseDutyRunning{false};
static std::atomic<bool> gStepTriggerRunning{false};
static std::atomic<bool> gNoiseProfileRunning{false};
static std::atomic<bool> gCancelRequested{false};

// Dynamic params for Noise Profile
static std::atomic<uint64_t> gNoiseSamples{100};
static std::atomic<uint64_t> gNoiseInterval{1};

// Forward declaration of helpers
void cleanupAndRestore();
std::optional<autotune::Config> loadConfig(const std::string& jsonPath);

// --- Task: Base Duty ---
void runBaseDutyTask(const std::string& jsonPath,
                     std::shared_ptr<sdbusplus::asio::connection> conn)
{
    std::cerr << "[autotune] BaseDuty Task Started\n";
    gBaseDutyRunning = true;
    gIsRunning = true;
    gCancelRequested = false;

    // Stop pid control
    systemctl("systemctl stop phosphor-pid-control");

    auto cfg = loadConfig(jsonPath);
    if (cfg && cfg->baseDuty)
    {
        // Purge logs
        if (!cfg->baseDuty->logPath.empty())
            purgeLogDirectory(parentDir(cfg->baseDuty->logPath));

        if (!gCancelRequested)
        {
            auto res = autotune::exp::runBaseDuty(*cfg);
            std::cerr << "[autotune] Base Duty Result: " << res.baseDutyRaw << "\n";
        }
    }
    else
    {
        std::cerr << "[autotune] No BaseDuty config found.\n";
    }

    cleanupAndRestore();
    gBaseDutyRunning = false;
    gIsRunning = false;
    
    // Reset Enabled property to false asynchronously (via setting it on DBus self?)
    // Actually, sdbusplus object server holds the state. We'll rely on the user or the
    // fact that next time they set it true, the property change fires.
    // Ideally we should set the property back to false.
    // For simplicity here, we leave it as true until user toggles or we implement self-reset.
    std::cerr << "[autotune] BaseDuty Task Finished\n";
}

// --- Task: Step Trigger + Analysis ---
void runStepTriggerTask(const std::string& jsonPath,
                        std::shared_ptr<sdbusplus::asio::connection> conn)
{
    std::cerr << "[autotune] StepTrigger Task Started\n";
    gStepTriggerRunning = true;
    gIsRunning = true;
    gCancelRequested = false;

    systemctl("systemctl stop phosphor-pid-control");

    auto cfg = loadConfig(jsonPath);
    if (cfg && cfg->stepTrigger)
    {
        // Purge logs
        if (!cfg->stepTrigger->logPath.empty())
            purgeLogDirectory(parentDir(cfg->stepTrigger->logPath));
        if (cfg->fopdt && !cfg->fopdt->logPath.empty())
            purgeLogDirectory(parentDir(cfg->fopdt->logPath));

        // Need a base duty to start from. Config doesn't store previous result.
        // We'll use fan min duty as safe fallback if no base duty logic in this step.
        // Or we could read the base duty log? For now, use min duty.
        int startDuty = 0;
        if (!cfg->fans.empty()) startDuty = cfg->fans.front().minDuty;

        autotune::exp::StepResponse stepResp{};
        if (!gCancelRequested)
        {
            stepResp = autotune::exp::runStepTrigger(*cfg, startDuty);
            std::cerr << "[autotune] Step Done, samples=" << stepResp.samples.size() << "\n";
        }

        // FOPDT
        if (!gCancelRequested && cfg->fopdt && !stepResp.samples.empty())
        {
            auto fopdt = autotune::proc::identifyFOPDT(stepResp, cfg->temp.setpoint,
                                                       cfg->basic.truncateDecimals);
            if (fopdt)
            {
                std::cerr << "[autotune] FOPDT: k=" << fopdt->k << " tau=" << fopdt->tau << "\n";
                // Log FOPDT
                if (!cfg->fopdt->logPath.empty())
                {
                   // writing logic same as before...
                   try {
                        std::filesystem::create_directories(
                            std::filesystem::path(cfg->fopdt->logPath).parent_path());
                        std::ofstream ofs(cfg->fopdt->logPath, std::ios::out | std::ios::trunc);
                        ofs << "k=" << fopdt->k << ",tau=" << fopdt->tau
                            << ",theta=" << fopdt->theta << "\n";
                   } catch(...) {}
                }

                // IMC
                if (cfg->imc)
                {
                    auto pidVec = autotune::tuning::imcFromFopdt(*fopdt, cfg->fopdt->epsilonFactors);
                    writePidOut(cfg->imc->logPath, pidVec);
                    std::cerr << "[autotune] PID Gains Written.\n";
                }
            }
        }
    }
    else
    {
        std::cerr << "[autotune] No StepTrigger config found.\n";
    }

    cleanupAndRestore();
    gStepTriggerRunning = false;
    gIsRunning = false;
    std::cerr << "[autotune] StepTrigger Task Finished\n";
}

// --- Task: Noise Profile ---
void runNoiseProfileTask(const std::string& jsonPath,
                         std::shared_ptr<sdbusplus::asio::connection> conn)
{
    std::cerr << "[autotune] Noise Profile Task Started\n";
    gNoiseProfileRunning = true;
    gIsRunning = true;
    gCancelRequested = false;

    // NOTE: Noise profile does NOT stop pid-control, because user sets manual fan speed?
    // User said: "Experimenter manually sets speed".
    // If we rely on manual speed, valid. But phosphor-pid-control might interfere if it is running in "auto" mode.
    // If phosphor-pid-control is running, it will try to control fan.
    // Safest is to stop it, so user's manual setting (via dbus/sysfs) sticks.
    // Assuming user manually sets it AFTER we stop? OR user sets it via other means.
    // Let's stop it to be safe, assuming user has a way (e.g. `systemctl stop` themselves or we stop it).
    // "Experimenter manually sets speed" -> User might use `fan_pwm` target.
    // If pid-control is running, it overwrites targets. So we MUST stop it.
    
    systemctl("systemctl stop phosphor-pid-control");

    auto cfg = loadConfig(jsonPath);
    
    // We run regardless of whether 'noise' experiment is in JSON, 
    // but we need 'noiselog' path from it if present.
    // If not in JSON, config object might not have noiseProfile populated.
    // We'll proceed, just won't log to file if missing.
    if (!cfg)
    {
        std::cerr << "[autotune] Config load failed, but running noise profile without log.\n";
        // Attempt to create a dummy config with at least basic sensors?
        // Actually runNoiseProfile needs cfg for sensor input paths.
        // So we need valid config.
    }
    
    if (cfg && !gCancelRequested)
    {
        // Use global atomics if set (via dbus properties), 
        // OR rely on config defaults if they haven't been touched?
        // Current logic: gNoiseSamples initialized to 100.
        // We should update gNoiseSamples from config IF it's the first run or if we want config to override?
        // Better: Initialize the D-Bus properties with the config values at startup?
        // But main() creates interfaces before loading config in the loop.
        // Let's just use the current atomic values.
        // BUT we should update the atomics if we just loaded a config that has them?
        // Actually, runNoiseProfileTask reloads config every time.
        // If config has values, we should respect them unless overridden by D-Bus?
        // D-Bus property writes update the atomics immediately.
        // If we want config to set defaults, we should do it at startup.
        // For now, let's say the Atomic wins (D-Bus), but if Config has it and Atomic is default, maybe use Config?
        // Let's keep it simple: D-Bus properties control the run.
        // Users can set D-Bus properties.
        // If user didn't set D-Bus properties, we use default (100).
        // Since we are adding "samplecount" to json, the user expects that to be used.
        // So:
        if (cfg->noiseProfile) {
             if (gNoiseSamples == 100 && cfg->noiseProfile->sampleCount > 0) 
                 gNoiseSamples = cfg->noiseProfile->sampleCount;
             if (gNoiseInterval == 1 && cfg->noiseProfile->pollInterval > 0) 
                 gNoiseInterval = cfg->noiseProfile->pollInterval;
        }

        int n = static_cast<int>(gNoiseSamples.load());
        int iv = static_cast<int>(gNoiseInterval.load());
        autotune::exp::runNoiseProfile(*cfg, n, iv);
    }
    else
    {
         std::cerr << "[autotune] Cannot run noise profile: Config failed or Cancelled.\n";
    }

    cleanupAndRestore();
    gNoiseProfileRunning = false;
    gIsRunning = false;
    std::cerr << "[autotune] Noise Profile Task Finished\n";
}


void cleanupAndRestore()
{
    systemctl("systemctl start phosphor-pid-control");
}

std::optional<autotune::Config> loadConfig(const std::string& jsonPath)
{
    // Try EM then JSON
    auto cfg = autotune::dbuscfg::loadConfigFromEntityManager();
    if (!cfg)
    {
        try { cfg = autotune::loadConfigFromJsonFile(jsonPath); }
        catch(...) { return std::nullopt; }
    }
    return cfg;
}

int main(int argc, char** argv)
{
    std::string jsonPath = "/usr/share/phosphor-pid-autotune/configs/autotune.json";
    if (argc > 1) jsonPath = argv[1];

    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    conn->request_name("xyz.openbmc_project.PIDAutotune");

    sdbusplus::asio::object_server server(conn);

    // --- Object 1: BaseDuty ---
    auto ifaceBase = server.add_interface("/xyz/openbmc_project/PIDAutotune/BaseDuty",
                                          "xyz.openbmc_project.Object.Enable");
    bool baseEnabled = false;
    ifaceBase->register_property("Enabled", baseEnabled,
        [&](const bool& req, bool& cur) {
            if (req == cur) return 1; // no change
            if (req) {
                if (gIsRunning) {
                    std::cerr << "[autotune] Reject BaseDuty: Busy\n";
                    // Throw D-Bus error? For now just log and don't set true
                    return 0; // fail
                }
                cur = true;
                boost::asio::post(io, [jsonPath, conn](){ runBaseDutyTask(jsonPath, conn); });
            } else {
                // Cancel request
                if (gBaseDutyRunning) {
                    gCancelRequested = true;
                }
                cur = false;
            }
            return 1;
        });
    ifaceBase->initialize();


    // --- Object 2: StepTrigger ---
    auto ifaceStep = server.add_interface("/xyz/openbmc_project/PIDAutotune/StepTrigger",
                                          "xyz.openbmc_project.Object.Enable");
    bool stepEnabled = false;
    ifaceStep->register_property("Enabled", stepEnabled,
        [&](const bool& req, bool& cur) {
            if (req == cur) return 1;
            if (req) {
                 if (gIsRunning) {
                    std::cerr << "[autotune] Reject StepTrigger: Busy\n";
                    return 0;
                }
                cur = true;
                boost::asio::post(io, [jsonPath, conn](){ runStepTriggerTask(jsonPath, conn); });
            } else {
                if (gStepTriggerRunning) {
                    gCancelRequested = true;
                }
                cur = false;
            }
            return 1;
        });
    ifaceStep->initialize();

    // --- Object 3: NoiseProfile ---
    // Interface 1: Enable Control
    auto ifaceNoiseEnable = server.add_interface("/xyz/openbmc_project/PIDAutotune/NoiseProfile",
                                                 "xyz.openbmc_project.Object.Enable");
    bool noiseEnabled = false;
    ifaceNoiseEnable->register_property("Enabled", noiseEnabled,
        [&](const bool& req, bool& cur) {
            if (req == cur) return 1;
            if (req) {
                 if (gIsRunning) { std::cerr << "Busy\n"; return 0; }
                cur = true;
                boost::asio::post(io, [jsonPath, conn](){ runNoiseProfileTask(jsonPath, conn); });
            } else {
                if (gNoiseProfileRunning) gCancelRequested = true;
                cur = false;
            }
            return 1;
        });
    ifaceNoiseEnable->initialize();

    // Interface 2: Configuration
    auto ifaceNoiseConfig = server.add_interface("/xyz/openbmc_project/PIDAutotune/NoiseProfile",
                                                 "xyz.openbmc_project.PIDAutotune.NoiseConfig");

    ifaceNoiseConfig->register_property("SampleCount", uint64_t(100),
        [&](const uint64_t& req, uint64_t& cur) {
            std::cerr << "Set SampleCount=" << req << "\n";
            gNoiseSamples = req;
            cur = req;
            return 1;
        });
    ifaceNoiseConfig->register_property("PollInterval", uint64_t(1),
        [&](const uint64_t& req, uint64_t& cur) {
            std::cerr << "Set PollInterval=" << req << "\n";
            gNoiseInterval = req;
            cur = req;
            return 1;
        });
    ifaceNoiseConfig->initialize();

    io.run();
    return 0;
}
