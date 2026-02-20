#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include <memory>

class SCHCAckOnErrorReceiver;   // ← forward declaration

class SCHCAckOnErrorReceiver_INIT: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorReceiver_INIT(SCHCAckOnErrorReceiver& ctx);
        ~SCHCAckOnErrorReceiver_INIT();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:

        SCHCAckOnErrorReceiver& _ctx;
};
