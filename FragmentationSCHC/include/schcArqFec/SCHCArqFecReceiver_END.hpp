#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include "schcArqFec/SCHCArqFecReceiver.hpp"
#include <spdlog/spdlog.h>
#include <memory>

class SCHCArqFecReceiver;   // ← forward declaration

class SCHCArqFecReceiver_END: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCArqFecReceiver_END(SCHCArqFecReceiver& ctx);
        ~SCHCArqFecReceiver_END();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:

        SCHCArqFecReceiver& _ctx;
};
