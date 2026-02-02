#pragma once

#include "ISCHCState.hpp"
#include "ISCHCStack.hpp"
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration

class SCHCAckOnErrorSender_ERROR: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_ERROR(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_ERROR();
        void execute(char* msg=nullptr, int len =-1) override;
        void release() override;
    
    private:
        SCHCAckOnErrorSender& _ctx;
};
