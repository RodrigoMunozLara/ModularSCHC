#pragma once

#include "ISCHCState.hpp"
#include "ISCHCStack.hpp"
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration
class SCHCSession;

class SCHCAckOnErrorSender_END: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_END(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_END();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        SCHCAckOnErrorSender& _ctx;
};
