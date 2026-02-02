#pragma once

#include "ICore.hpp"
#include "Orchestrator.hpp"
#include "Types.hpp"
#include "ConfigStructs.hpp"
#include "SCHCLoRaWANStack.hpp"

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
        void enqueueFromStack(std::unique_ptr<RoutedMessage> msg) override;
        void start() override;
        void stop() override;

    private:
        void runTx() override;
        void runRx() override;
        SCHCSession* findUnassignedSession(std::map<uint32_t, std::unique_ptr<SCHCSession>>& sessions);

    private:

        Orchestrator&   orchestrator;
        AppConfig       _appConfig;

        std::atomic<bool> running{false};

        std::thread rxThread;
        std::thread txThread;

        // Queue for messages from the Orchestrator
        std::queue<std::unique_ptr<RoutedMessage>> txQueue;
        std::mutex txMtx;
        std::condition_variable txCv;

        // Queue for messages from the Protocol
        std::queue<std::unique_ptr<RoutedMessage>> protoQueue;
        std::mutex protoMtx;
        std::condition_variable protoCv;

        /* The first argument is the Dtag. Protocols that do not 
        use Dtag MUST define a unique identifier for each session. 
        For example, LoRaWAN uses fPort. */
        std::mutex sessionsMtx;
        std::map<uint32_t, std::unique_ptr<SCHCSession>> uplinkSessions;
        std::map<uint32_t, std::unique_ptr<SCHCSession>> downlinkSessions; 

    public:
        std::unique_ptr<ISCHCStack> _stack;
};