#pragma once

#include "interfaces/ICore.hpp"
#include "Orchestrator.hpp"
#include "Types.hpp"
#include "ConfigStructs.hpp"
#include "stacks/SCHCLoRaWANStack.hpp"
#include "stacks/SCHCLoRaWAN_NS_MQTT_Stack.hpp"

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>
#include <memory>
#include <vector>
#include "spdlog/spdlog.h"

class SCHCSession;  // forward declaration

class SCHCCore : public ICore
{
    public:
        SCHCCore(Orchestrator& orchestrator, AppConfig& appConfig);
        ~SCHCCore() override;
        CoreId id() override;
        void enqueueFromOrchestator(std::unique_ptr<RoutedMessage> msg) override;
        void enqueueFromStack(std::unique_ptr<StackMessage> msg);
        void start() override;
        void stop() override;
    private:
        void runTx() override;
        void runRx() override;
        void runCleaner() override;


        Orchestrator&   orchestrator;
        AppConfig       _appConfig;

        std::atomic<bool> running{false};

        std::thread rxThread;
        std::thread txThread;
        std::thread cleanerThread;

        // Queue for messages from the Orchestrator
        std::queue<std::unique_ptr<RoutedMessage>> txQueue;
        std::mutex txMtx;
        std::condition_variable txCv;

        // Queue for messages from the Protocol
        std::queue<std::unique_ptr<StackMessage>> protoQueue;
        std::mutex protoMtx;
        std::condition_variable protoCv;

        /* The first argument is the Dtag. Protocols that do not 
        use Dtag MUST define a unique identifier for each session. 
        For example, LoRaWAN uses fPort. */
        std::mutex sessionsMtx;
        std::map<uint8_t, std::unique_ptr<SCHCSession>> uplinkSessions;
        std::map<uint8_t, std::unique_ptr<SCHCSession>> downlinkSessions;
        uint8_t     _uplinkSessionCounter;
        uint8_t     _uplinkSessionCounterMax;
        uint8_t     _downlinkSessionCounter;
        uint8_t     _downlinkSessionCounterMax;

        std::mutex cleanerMtx;
        std::condition_variable cleanerCv;

    public:
        std::unique_ptr<ISCHCStack> _stack;
};