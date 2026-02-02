#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "ConfigManager.hpp"
#include "Orchestrator.hpp"
#include "BackhaulCore.hpp"
#include "SCHCCore.hpp"


std::atomic<bool> globalRunning{true};

void handleSignal(int sig)
{
    globalRunning.store(false);
    write(STDERR_FILENO, "SIGINT received\n", 16);
}

int main()
{
    /*
    main()
    ├─> read and load configuration file (paths, stack parameters, threads, firewall rules)
    ├─> initialise global logging (file, console, levels)
    ├─> create Orchestrator
    ├─> create Cores (Backhaul, SCHC) with reference to Orchestrator
    ├─> register Cores in Orchestrator
    ├─> launch RX/TX threads for each core
    └─> start main loop or wait for threads
    */

    /* Reading and Loading configuration file */
    AppConfig appConfig;
    std::string configFile = "config_frag_node.json";
    if (!loadConfig(configFile, appConfig))
    {
        std::cerr << "Error reading configuration file: " << configFile << "\n";
        return 1;
    }

    spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
    spdlog::set_level(parseLogLevel(appConfig.logging.log_level));

    SPDLOG_CRITICAL("Logging initialized with level: {}", appConfig.logging.log_level);

    /* Create Orchestrator */
    Orchestrator orchestrator;

    /* Create Cores (Ethernet, LoRa) with reference to Orchestrator*/
    BackhaulCore backhaulCore(orchestrator, appConfig);
    SCHCCore schcCore(orchestrator, appConfig);

    /* Register Cores in Orchestrator */
    orchestrator.registerCore(CoreId::BACKHAUL, &backhaulCore);
    orchestrator.registerCore(CoreId::SCHC, &schcCore);

    /* Starting Stacks*/
    backhaulCore.start();
    schcCore.start();

    
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    while (globalRunning.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    backhaulCore.stop();
    schcCore.stop();


}