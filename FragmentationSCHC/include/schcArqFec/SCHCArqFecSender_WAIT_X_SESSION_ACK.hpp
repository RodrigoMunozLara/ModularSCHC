#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include "schcArqFec/SCHCArqFecSender.hpp"
#include "schcAckOnError/SCHCNodeMessage.hpp"
#include <spdlog/spdlog.h>
#include "spdlog/fmt/bin_to_hex.h"
#include <memory>



class SCHCArqFecSender;   // ← forward declaration

class SCHCArqFecSender_WAIT_X_SESSION_ACK: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCArqFecSender_WAIT_X_SESSION_ACK(SCHCArqFecSender& ctx);
        ~SCHCArqFecSender_WAIT_X_SESSION_ACK();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:

        SCHCArqFecSender& _ctx;
};
