#pragma once

class ISCHCState
{
    public:
        virtual void execute(char* msg=nullptr, int len =-1) = 0;
        virtual void release() = 0;
};