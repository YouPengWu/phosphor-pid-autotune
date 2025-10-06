#include "PID_tuning_methods/imc.hpp"
#include "buildjson/buildjson.hpp"
#include "core/numeric.hpp"
#include "dbus/constants.hpp"
#include "dbus/dbusconfiguration.hpp"
#include "experiment/base_duty.hpp"
#include "experiment/step_trigger.hpp"
#include "process_models/fopdt.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp> // keep this; avoid rules.hpp

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Write PID results to a file. Creates parent directories if needed.
static void writePidOut(const std::string& path,
                        const std::map<double, autotune::tuning::PidGains>& m)
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
    for (const auto& [lam, g] : m)
    {
        ofs << "lambda=" << lam << ",Kp=" << g.Kp << ",Ki=" << g.Ki
            << ",Kd=" << g.Kd << "\n";
    }
}

// Fire-and-forget systemctl call.
static void systemctl(const char* cmd)
{
    int rc = std::system(cmd);
    (void)rc;
}

// Sanity-check all sensor paths before running experiments.
// If any required path is missing, log and return false to abort early.
static bool checkSensorPaths(const autotune::Config& cfg)
{
    bool ok = true;

    auto check = [&](const std::string& p, const char* tag) {
        if (p.empty() || !std::filesystem::exists(p))
        {
            std::cerr << "[autotune] MISSING " << tag << ": " << p << "\n";
            ok = false;
        }
    };

    // Temperature input path
    check(cfg.temp.inputPath, "temp input");

    // Each fan pwm/tach path
    for (const auto& f : cfg.fans)
    {
        check(f.pwmPath, "fan pwm");
        check(f.tachPath, "fan tach");
    }

    return ok;
}

int main(int argc, char** argv)
{
    // Fallback JSON when EntityManager has no data.
    std::string jsonPath =
        "/usr/share/phosphor-pid-autotune/configs/autotune.json";
    if (argc > 1)
    {
        jsonPath = argv[1];
    }

    // D-Bus connection and service name.
    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    conn->request_name(autotune::dbusconst::kEnableService);

    // Expose xyz.openbmc_project.Object.Enable as a simple readWrite property.
    sdbusplus::asio::object_server server(conn);
    auto iface = server.add_interface(autotune::dbusconst::kEnablePath,
                                      autotune::dbusconst::kEnableIface);

    bool enabled = false; // local cached value (for debug/log)
    bool running = false; // re-entrancy guard
    std::atomic<bool> cancelRequested{false}; // cooperative cancel flag

    // Register readWrite (sdbusplus manages the value and emits
    // PropertiesChanged).
    iface->register_property("Enabled", enabled,
                             sdbusplus::asio::PropertyPermission::readWrite);

    // Publish the interface.
    iface->initialize();

    // Explicitly publish false (emits PropertiesChanged).
    try
    {
        enabled = false;
        iface->set_property("Enabled", enabled);
        std::cerr << "[autotune] initialized (Enabled=false)\n";
    }
    catch (...)
    {
        std::cerr << "[autotune] WARN: initial set_property(false) failed\n";
    }

    // Helper: cancellation check used inside runOnce().
    auto shouldCancel = [&]() -> bool {
        if (cancelRequested.load(std::memory_order_relaxed))
        {
            std::cerr << "[autotune] cancellation requested; exiting run\n";
            return true;
        }
        return false;
    };

    // The main autotune pipeline (runs once per trigger).
    auto runOnce = [&]() {
        if (running)
        {
            std::cerr << "[autotune] runOnce() ignored: already running\n";
            return;
        }
        running = true;
        cancelRequested.store(false, std::memory_order_relaxed);
        std::cerr << "[autotune] runOnce() started\n";

        // Always bring pid-control back; clear running and (re)publish false.
        struct ScopeExit
        {
            std::function<void()> f;
            ~ScopeExit()
            {
                if (f)
                    f();
            }
        } guard{[&] {
            // Ensure stock controller is active after we exit (idempotent).
            systemctl("systemctl start phosphor-pid-control");

            running = false;

            // Publish false at the end (safe even if already false).
            try
            {
                enabled = false;
                iface->set_property("Enabled", enabled);
                std::cerr << "[autotune] set Enabled=false (end of run)\n";
            }
            catch (...)
            {
                std::cerr << "[autotune] WARN: set Enabled=false failed\n";
            }

            std::cerr << "[autotune] runOnce() finished\n";
        }};

        // Stop the stock control loop first (unless cancelled before we start).
        if (shouldCancel())
            return;
        systemctl("systemctl stop phosphor-pid-control");

        // Load configuration (EntityManager first, then JSON).
        if (shouldCancel())
            return;
        std::optional<autotune::Config> cfg =
            autotune::dbuscfg::loadConfigFromEntityManager();

        if (!cfg)
        {
            try
            {
                cfg = autotune::loadConfigFromJsonFile(jsonPath);
            }
            catch (const std::exception& e)
            {
                std::cerr << "[autotune] Config error: " << e.what() << "\n";
                return; // ScopeExit will restart pid-control & reset Enabled
            }
        }

        // Abort early if sensors are missing to avoid infinite loops in
        // experiments.
        if (!checkSensorPaths(*cfg))
        {
            std::cerr << "[autotune] abort: sensor paths missing\n";
            return;
        }

        if (shouldCancel())
            return;

        // Experiments (by priority). Start with a reasonable base.
        int baseDutyRaw = 0;
        if (!cfg->fans.empty())
        {
            baseDutyRaw = cfg->fans.front().minDuty;
        }

        if (cfg->baseDuty && cfg->baseDuty->enabled)
        {
            if (shouldCancel())
                return;
            auto res = autotune::exp::runBaseDuty(*cfg);
            baseDutyRaw = res.baseDutyRaw;
            std::cerr << "[autotune] baseDutyRaw=" << baseDutyRaw << "\n";
        }

        autotune::exp::StepResponse stepResp{};
        if (cfg->stepTrigger && cfg->stepTrigger->enabled)
        {
            if (shouldCancel())
                return;
            stepResp = autotune::exp::runStepTrigger(*cfg, baseDutyRaw);
            std::cerr << "[autotune] stepTrigger done, samples="
                      << stepResp.samples.size() << "\n";
        }

        // Process model (FOPDT).
        std::optional<autotune::proc::FopdtParams> fopdt;
        if (cfg->fopdt && cfg->fopdt->enabled)
        {
            if (shouldCancel())
                return;
            fopdt = autotune::proc::identifyFOPDT(stepResp, cfg->temp.setpoint,
                                                  cfg->basic.truncateDecimals);

            if (fopdt)
            {
                std::cerr << "[autotune] FOPDT: K=" << fopdt->K
                          << " T=" << fopdt->T << " L=" << fopdt->L << "\n";
            }

            if (shouldCancel())
                return;

            if (fopdt && !cfg->fopdt->logPath.empty())
            {
                try
                {
                    std::filesystem::create_directories(
                        std::filesystem::path(cfg->fopdt->logPath)
                            .parent_path());
                    std::ofstream ofs(cfg->fopdt->logPath,
                                      std::ios::out | std::ios::trunc);
                    ofs << "K=" << fopdt->K << ",T=" << fopdt->T
                        << ",L=" << fopdt->L << "\n";
                }
                catch (...)
                {
                    std::cerr << "[autotune] write fopdt log failed\n";
                }
            }
        }

        // PID tuning (IMC for each lambda).
        if (fopdt && cfg->imc && cfg->imc->enabled)
        {
            if (shouldCancel())
                return;
            auto pidmap = autotune::tuning::imcFromFopdt(
                *fopdt, cfg->fopdt->lambdaFactors);
            writePidOut(cfg->imc->logPath, pidmap);
            std::cerr << "[autotune] IMC PID gains written\n";
        }
    };

    // Subscribe to PropertiesChanged using a raw match rule string (sa{sv}as).
    // We only care about our object path and the Properties interface.
    auto& rawbus = static_cast<sdbusplus::bus_t&>(*conn);
    const std::string matchRule =
        "type='signal',"
        "interface='org.freedesktop.DBus.Properties',"
        "member='PropertiesChanged',"
        "path='" +
        std::string(autotune::dbusconst::kEnablePath) + "'";

    sdbusplus::bus::match_t matcher(
        rawbus, matchRule.c_str(), [&](sdbusplus::message_t& msg) {
            std::string ifaceName;
            // Use std::variant for broad compatibility across sdbusplus
            // versions.
            std::map<std::string,
                     std::variant<bool, int64_t, double, std::string>>
                changed;
            std::vector<std::string> invalidated;
            try
            {
                // Signature: sa{sv}as
                msg.read(ifaceName, changed, invalidated);
            }
            catch (const std::exception& e)
            {
                std::cerr << "[autotune] match read error: " << e.what()
                          << "\n";
                return;
            }

            // Only react to our Enable interface
            if (ifaceName != std::string(autotune::dbusconst::kEnableIface))
                return;

            auto it = changed.find("Enabled");
            if (it == changed.end())
                return;

            if (auto pval = std::get_if<bool>(&it->second))
            {
                enabled = *pval; // update local cache mainly for debug logs
                std::cerr << "[autotune] PropertiesChanged: Enabled="
                          << (enabled ? "true" : "false") << "\n";

                if (enabled)
                {
                    if (!running)
                        boost::asio::post(io, runOnce);
                    else
                        std::cerr
                            << "[autotune] already running; new true ignored\n";
                }
                else
                {
                    // Mid-run OFF: request cancellation and immediately
                    // hand control back to phosphor-pid-control.
                    cancelRequested.store(true, std::memory_order_relaxed);
                    systemctl("systemctl start phosphor-pid-control");
                }
            }
        });

    // Enter event loop.
    io.run();
    return 0;
}
