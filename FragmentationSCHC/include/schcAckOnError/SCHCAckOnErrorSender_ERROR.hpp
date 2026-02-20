#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration
class SCHCSession;

class SCHCAckOnErrorSender_ERROR: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_ERROR(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_ERROR();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        SCHCAckOnErrorSender& _ctx;
};
