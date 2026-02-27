#pragma once

#include "ConfigStructs.hpp"
#include "interfaces/ISCHCState.hpp"
#include "Types.hpp"

class ISCHCStateMachine
{
    public:
        virtual ~ISCHCStateMachine() = default;
        virtual void execute(const std::vector<uint8_t>& msg = {}) = 0;
        virtual void timerExpired() = 0;
        virtual void release() = 0;
        virtual void executeAgain() = 0;
        virtual void executeTimer(int delay) = 0;
        virtual void enqueueTimer() = 0;
    private:
        ISCHCState* estado = nullptr;
};
