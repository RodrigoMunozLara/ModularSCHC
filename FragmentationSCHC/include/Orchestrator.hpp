#pragma once

#include "ICore.hpp"
#include "Types.hpp"

#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <spdlog/spdlog.h>

class Orchestrator
{
    public:
        Orchestrator();
        ~Orchestrator();
        void onMessageFromCore(std::unique_ptr<RoutedMessage> msg);
        void registerCore(CoreId id, ICore* core);
        void stop();

    private:
        CoreId selectDestination(const RoutedMessage& msg);
        void forwardToCore(CoreId dst, std::unique_ptr<RoutedMessage> msg);

        std::unordered_map<CoreId, ICore*> cores;
        std::mutex mtx;
};