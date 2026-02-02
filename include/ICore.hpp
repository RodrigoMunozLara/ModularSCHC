#pragma once

#include "Types.hpp"

class ICore
{
    public:
        virtual ~ICore() = default;
        virtual CoreId id() = 0;
        virtual void enqueueFromOrchestator(std::unique_ptr<RoutedMessage> msg) = 0;
        virtual void enqueueFromStack(std::unique_ptr<RoutedMessage> msg) = 0;
        virtual void start() = 0;
        virtual void stop() = 0;
    private:
        virtual void runTx() = 0;
        virtual void runRx() = 0;
};
