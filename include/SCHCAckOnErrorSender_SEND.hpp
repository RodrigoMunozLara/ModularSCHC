#pragma once

#include "ISCHCState.hpp"
#include "ISCHCStack.hpp"
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration

class SCHCAckOnErrorSender_SEND: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_SEND(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_SEND();
        void execute(char* msg=nullptr, int len =-1) override;
        void release() override;
    
    private:
        SCHCAckOnErrorSender& _ctx;
};
