#pragma once

#include "ISCHCState.hpp"
#include "ISCHCStack.hpp"
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration

class SCHCAckOnErrorSender_END: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_END(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_END();
        void execute(char* msg=nullptr, int len =-1) override;
        void release() override;
    
    private:
        SCHCAckOnErrorSender& _ctx;
};
