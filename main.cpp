#include "buildjson/config.hpp"
#include "experiment/step_trigger.hpp"

#include <boost/asio.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>

#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::cout << "Starting phosphor-pid-autotune...\n";

    std::string configPath = "/etc/phosphor-pid-autotune/autotune.json";
    if (argc > 1 && argv[1] != nullptr)
    {
        configPath = argv[1];
    }
    else if (std::filesystem::exists("configs/autotune.json"))
    {
        configPath = "configs/autotune.json";
    }

    auto cfg = autotune::config::loadConfig(configPath);
    std::cerr << "Loaded " << cfg.experiments.size() << " experiments from "
              << configPath << "\n";
    if (cfg.experiments.empty())
    {
        std::cerr << "No experiments configured.\n";
    }

    auto io = std::make_shared<boost::asio::io_context>();
    auto conn = std::make_shared<sdbusplus::asio::connection>(*io);

    conn->request_name("xyz.openbmc_project.PIDAutotune");

    auto server = std::make_shared<sdbusplus::asio::object_server>(conn);

    server->add_manager("/xyz/openbmc_project/PIDAutotune");

    sdbusplus::bus_t& busRef = static_cast<sdbusplus::bus_t&>(*conn);

    // Captured by shared_ptr to keep alive in lambdas? No, vector of
    // shared_ptrs. We need to ensure the vector persists. It is in main scope.
    // However, the lambda captures it by reference.

    std::vector<std::shared_ptr<autotune::experiment::StepTrigger>> experiments;

    for (const auto& expCfg : cfg.experiments)
    {
        std::string objPath =
            "/xyz/openbmc_project/PIDAutotune/" + expCfg.tempSensor;

        auto exp = std::make_shared<autotune::experiment::StepTrigger>(
            busRef, objPath, cfg.basic, expCfg);
        experiments.push_back(exp);

        auto iface = server->add_interface(
            objPath, "xyz.openbmc_project.PIDAutotune.steptrigger");

        iface->register_property(
            "Enabled", false,
            [exp](const bool& req, bool& curr) {
                if (req != curr)
                {
                    std::cerr << "[StepTrigger] Individual Enabled set to "
                              << req << "\n";
                    exp->setEnabled(req);
                    curr = req;
                }
                return 1;
            },
            [exp](const bool& curr) {
                (void)curr;
                return exp->getEnabled();
            });

        iface->initialize();
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> allTempsIface;
    bool allEnabled = false;

    int currentExpIdx = -1; // -1 means no sequence running

    {
        std::string objPath = "/xyz/openbmc_project/PIDAutotune/alltempsensor";
        allTempsIface = server->add_interface(
            objPath, "xyz.openbmc_project.PIDAutotune.steptrigger");

        allTempsIface->register_property(
            "Enabled", false,
            [&experiments, &allEnabled,
             &currentExpIdx](const bool& req, bool& curr) {
                if (req == curr)
                    return 1;
                curr = req;
                allEnabled = req;

                std::cerr << "[AllTempSensor] Enabled set to " << req
                          << ". Experiment count: " << experiments.size()
                          << "\n";

                if (req)
                {
                    // Start Sequence
                    currentExpIdx = 0;
                    if (!experiments.empty())
                    {
                        std::cerr
                            << "[AllTempSensor] Starting sequence with experiment 0\n";
                        experiments[0]->setEnabled(true);
                    }
                    else
                    {
                        std::cerr << "[AllTempSensor] No experiments to run.\n";
                        allEnabled = false;
                        curr = false;
                        currentExpIdx = -1;
                    }
                }
                else
                {
                    // Stop All
                    currentExpIdx = -1;
                    for (auto& e : experiments)
                        e->setEnabled(false);
                }
                return 1;
            });
        allTempsIface->initialize();
    }

    auto timer = std::make_shared<boost::asio::steady_timer>(*io);

    std::function<void(const boost::system::error_code&)> tick;
    tick = [&](const boost::system::error_code& ec) {
        if (ec)
            return;

        bool anyRunning = false;
        for (auto& exp : experiments)
        {
            exp->tick();
            if (exp->getEnabled())
                anyRunning = true;
        }

        // Sequential Logic Manager
        if (allEnabled && currentExpIdx >= 0)
        {
            // Check if current experiment is still running
            if (currentExpIdx < (int)experiments.size())
            {
                if (!experiments[currentExpIdx]->getEnabled())
                {
                    // Current finished, start next
                    currentExpIdx++;
                    if (currentExpIdx < (int)experiments.size())
                    {
                        std::cerr
                            << "[AllTempSensor] Starting next experiment: "
                            << currentExpIdx << "\n";
                        experiments[currentExpIdx]->setEnabled(true);
                    }
                    else
                    {
                        // End of sequence
                        std::cerr
                            << "[AllTempSensor] All experiments finished. Sequence complete.\n";
                        allEnabled = false;
                        allTempsIface->set_property("Enabled", false);
                        currentExpIdx = -1;
                    }
                }
            }
        }
        else if (allEnabled && !anyRunning && currentExpIdx == -1)
        {
            // Fallback for safety if somehow state gets weird, roughly original
            // logic but stricter
            std::cerr
                << "[AllTempSensor] No experiments running and logic reset. Resetting Enabled.\n";
            allEnabled = false;
            allTempsIface->set_property("Enabled", false);
        }

        timer->expires_after(std::chrono::milliseconds(100));
        timer->async_wait(tick);
    };

    timer->expires_after(std::chrono::milliseconds(100));
    timer->async_wait(tick);

    std::cout << "Service started.\n";
    io->run();

    return 0;
}
