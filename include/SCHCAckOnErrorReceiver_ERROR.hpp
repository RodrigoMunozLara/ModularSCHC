#pragma once

#include "ISCHCState.hpp"
#include "ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include <memory>

class SCHCAckOnErrorReceiver;   // ← forward declaration

class SCHCAckOnErrorReceiver_ERROR: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorReceiver_ERROR(SCHCAckOnErrorReceiver& ctx);
        ~SCHCAckOnErrorReceiver_ERROR();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        void        divideInTiles(char *buffer, int len);
        uint32_t    calculate_crc32(const char *data, size_t length); 

        SCHCAckOnErrorReceiver& _ctx;
};
