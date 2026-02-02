#pragma once

#include "ISCHCState.hpp"
#include "ISCHCStack.hpp"
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration

class SCHCAckOnErrorSender_WAIT_X_ACK: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_WAIT_X_ACK(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_WAIT_X_ACK();
        void execute(char* msg=nullptr, int len =-1) override;
        void release() override;
    
    private:
        SCHCAckOnErrorSender& _ctx;
};
