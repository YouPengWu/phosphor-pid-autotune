#include "step_trigger.hpp"
#include "../core/dbus_io.hpp"
#include "../core/utils.hpp"
#include "../process_models/fopdt.hpp"
#include "../PID_tuning_methods/imc.hpp"

#include <filesystem>
#include <iostream>
#include <cmath>

namespace autotune::experiment
{

namespace fs = std::filesystem;

StepTrigger::StepTrigger(sdbusplus::bus_t& b, const std::string& objectPath, // Match definition
                         const config::BasicSetting& basic,
                         const config::ExperimentConfig& exp,
                         const config::ModelConfig& model) :
    bus(b), objectPath(objectPath), basicCfg(basic), expCfg(exp), modelCfg(model)
{
}

void StepTrigger::setEnabled(bool enable)
{
    if (enabled == enable) return;
    enabled = enable;
    if (enabled) start(); else stop();
}

void StepTrigger::start()
{
    if (running) return; // Prevent double start
    
    running = true;
    state = State::InitialWait;
    currentIteration = 0;
    history.clear();
    fullLog.clear();
    startTime = std::chrono::steady_clock::now();
    lastTickTime = std::chrono::steady_clock::now();

    logDir = "/var/lib/phosphor-pid-autotune/log/" + expCfg.tempSensor;
    std::cerr << "[StepTrigger] Starting " << expCfg.tempSensor << " LogDir: " << logDir << "\n";
    try {
        if (fs::create_directories(logDir)) {
             std::cerr << "[StepTrigger] Created log directory: " << logDir << "\n";
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating log dir: " << e.what() << "\n";
    }

    std::string filename = logDir + "/step_trigger_" + expCfg.tempSensor + ".txt";
    logFile.open(filename, std::ios::out | std::ios::trunc);
    logFile << "n,time,temp,pwm,slope,rmse,mean_temp\n";

    // Start plot logger
    plotLogger.start(logDir, expCfg.tempSensor);

    dbusio::writePwmAllByInput(expCfg.initialFanSensors, expCfg.initialPwmDuty);
    
    std::cerr << "[StepTrigger] Started " << expCfg.tempSensor << " Initial PWM: " << expCfg.initialPwmDuty << "\n";
}

void StepTrigger::stop()
{
    running = false;
    state = State::Idle;
    if (logFile.is_open()) logFile.close();
    plotLogger.close(); // Close plot file
}

void StepTrigger::tick()
{
    if (!running) return;

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - lastTickTime;

    if (elapsed.count() < basicCfg.pollInterval) return;
    lastTickTime = now;

    iteration();
}

void StepTrigger::iteration()
{
    double temp = dbusio::readTempCByInput(expCfg.tempSensor);
    double currentPwm = (state == State::InitialWait) ? expCfg.initialPwmDuty : expCfg.afterTriggerPwmDuty;
    
    std::chrono::duration<double> t_diff = std::chrono::steady_clock::now() - startTime;
    double timestamp = t_diff.count();
    
    DataPoint dp{currentIteration, timestamp, temp, currentPwm, 0, 0, 0};
    history.push_back(dp);
    
    size_t win = static_cast<size_t>(basicCfg.windowSize);
    std::vector<double> histTemp, histTime;
    size_t startIdx = (history.size() > win) ? (history.size() - win) : 0;
    
    for (size_t i = startIdx; i < history.size(); ++i) {
        histTemp.push_back(history[i].temp);
        histTime.push_back(history[i].time);
    }
    
    dp.slope = core::calculateSlope(histTemp, histTime, win);
    dp.rmse = core::calculateRMSE(histTemp, win);
    dp.mean = core::calculateMean(histTemp, win);

    fullLog.push_back(dp);
    
    logFile << dp.n << "," << dp.time << "," << dp.temp << "," << dp.pwm << "," 
            << dp.slope << "," << dp.rmse << "," << dp.mean << "\n";
    logFile.flush();
    
    // Continuous Plot Logging
    int rate = basicCfg.plotSamplingRate;
    if (rate <= 0) rate = 1;
    if (dp.n % rate == 0)
    {
        plotLogger.log(dp.n, dp.pwm, dp.temp);
    }

    currentIteration++;

    if (state == State::InitialWait)
    {
        if (currentIteration >= expCfg.initialIterations) state = State::trigger;
    }
    else if (state == State::AfterTriggerWait)
    {
        if (currentIteration >= (expCfg.initialIterations + expCfg.afterTriggerIterations))
        {
            state = State::Finished;
            finishExperiment();
        }
    }
    
    if (state == State::trigger)
    {
        std::cout << "[StepTrigger] Triggering step for " << expCfg.tempSensor << "\n";
        dbusio::writePwmAllByInput(expCfg.afterTriggerFanSensors, expCfg.afterTriggerPwmDuty);
        state = State::AfterTriggerWait;
    }
}

void StepTrigger::finishExperiment()
{
    std::cout << "[StepTrigger] Finished " << expCfg.tempSensor << "\n";
    logFile.close();
    plotLogger.close(); // Ensure output is saved
    runAnalysis();
    enabled = false;
    running = false;
}

void StepTrigger::runAnalysis()
{
    runNoiseAnalysis(expCfg.tempSensor);
    runFOPDTAnalysis(expCfg.tempSensor);
    runIMCPIDAnalysis(expCfg.tempSensor);
}

void StepTrigger::runNoiseAnalysis(const std::string& sensorName)
{
    std::string filename = logDir + "/noise_" + sensorName + ".txt";
    std::ofstream noiseFile(filename);
    
    size_t win = basicCfg.windowSize;
    if (fullLog.size() < win) return; 

    size_t beforeIdx = (expCfg.initialIterations > 0 && expCfg.initialIterations < (int)fullLog.size()) 
                       ? (expCfg.initialIterations - 1) : 0;
    
    DataPoint beforeTrig = fullLog[beforeIdx];
    DataPoint endExp = fullLog.back();
    
    noiseFile << "Name:" << sensorName << "\n";
    noiseFile << "Iterations=" << fullLog.size() << "\n";
    noiseFile << "Pollinterval=" << basicCfg.pollInterval << "\n\n";
    
    noiseFile << "----Before step trigger------\n";
    noiseFile << "Slope=" << beforeTrig.slope << "\n";
    noiseFile << "RMSE=" << beforeTrig.rmse << "\n";
    noiseFile << "Mean=" << beforeTrig.mean << "\n\n";
    
    noiseFile << "----After step trigger------\n";
    noiseFile << "Slope=" << endExp.slope << "\n";
    noiseFile << "RMSE=" << endExp.rmse << "\n";
    noiseFile << "Mean=" << endExp.mean << "\n";
}

void StepTrigger::runFOPDTAnalysis(const std::string& sensorName)
{
    std::vector<double> times, temps;
    for(const auto& dp : fullLog) {
        times.push_back(dp.time);
        temps.push_back(dp.temp);
    }
    
    double stepTime = 0;
    if (expCfg.initialIterations < (int)fullLog.size()) {
        stepTime = fullLog[expCfg.initialIterations].time;
    }
    
    auto params = process_models::identifyFOPDT(times, temps, 
                                                expCfg.initialPwmDuty, expCfg.afterTriggerPwmDuty, 
                                                stepTime);

    std::string filename = logDir + "/fopdt_" + sensorName + ".txt";
    std::ofstream fFile(filename);
    fFile << "Name:" << sensorName << "\n";
    fFile << "k=" << params.k << "\n";
    fFile << "tau=" << params.tau << "\n";
    fFile << "theta=" << params.theta << "\n";
}

void StepTrigger::runIMCPIDAnalysis(const std::string& sensorName)
{
    std::vector<double> times, temps;
    for(const auto& dp : fullLog) {
        times.push_back(dp.time);
        temps.push_back(dp.temp);
    }
    double stepTime = 0;
    if (expCfg.initialIterations < (int)fullLog.size()) {
        stepTime = fullLog[expCfg.initialIterations].time;
    }
    auto params = process_models::identifyFOPDT(times, temps, 
                                                expCfg.initialPwmDuty, expCfg.afterTriggerPwmDuty, 
                                                stepTime);
                                                
    auto results = tuning::calculateIMC(params, modelCfg.epsilonOverTheta);

    std::string filename = logDir + "/imc_" + sensorName + ".txt";
    std::ofstream iFile(filename);
    iFile << "epsilon,epsilon_over_theta,type,Kp,Ki,Kd\n";
    for(const auto& r : results)
    {
        iFile << r.epsilon << "," << r.ratio << "," << r.type << ","
              << r.kp << "," << r.ki << "," << r.kd << "\n";
    }
}

} // namespace autotune::experiment
