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
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        void        divideInTiles(const std::vector<uint8_t>& msg);
        uint32_t    calculate_crc32(const std::vector<uint8_t>& msg); 

        SCHCAckOnErrorSender& _ctx;
};
