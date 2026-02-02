#pragma once

#include "ConfigStructs.hpp"
#include "ISCHCState.hpp"
#include "Types.hpp"

class ISCHCStateMachine
{
    public:
        virtual ~ISCHCStateMachine() = default;
        virtual void start(char* schc_packet, int len) = 0;
        virtual void execute(char* msg = nullptr, int len =-1) = 0;
        virtual void release() = 0;
        virtual void setState(SCHCAckOnErrorSenderStates state) = 0;
    private:
        ISCHCState* estado = nullptr;
};
