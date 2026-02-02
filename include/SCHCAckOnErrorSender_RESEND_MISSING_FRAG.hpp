#pragma once

#include "ISCHCState.hpp"
#include "ISCHCStack.hpp"
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration

class SCHCAckOnErrorSender_RESEND_MISSING_FRAG: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_RESEND_MISSING_FRAG(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_RESEND_MISSING_FRAG();
        void execute(char* msg=nullptr, int len =-1) override;
        void release() override;
    
    private:
        SCHCAckOnErrorSender& _ctx;
};
