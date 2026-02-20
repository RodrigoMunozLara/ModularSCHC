#pragma once

#include "schcAckOnError/SCHCAckOnErrorSender.hpp"
#include "schcAckOnError/SCHCNodeMessage.hpp"

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration

class SCHCAckOnErrorSender_WAIT_X_ACK: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_WAIT_X_ACK(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_WAIT_X_ACK();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        SCHCAckOnErrorSender& _ctx;
};
