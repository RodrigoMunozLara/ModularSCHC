#pragma once

#include "ISCHCState.hpp"
#include "ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include <memory>

class SCHCAckOnErrorSender;   // ← forward declaration

class SCHCAckOnErrorSender_INIT: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_INIT(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_INIT();
        void execute(char* msg=nullptr, int len =-1) override;
        void release() override;
    
    private:
        void        divideInTiles(char *buffer, int len);
        uint32_t    calculate_crc32(const char *data, size_t length); 

        SCHCAckOnErrorSender& _ctx;
};
