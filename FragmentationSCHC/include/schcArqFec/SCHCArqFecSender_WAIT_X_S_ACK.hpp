#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include "spdlog/fmt/bin_to_hex.h"
#include <memory>



class SCHCArqFecSender;   // ← forward declaration

class SCHCArqFecSender_WAIT_X_S_ACK: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCArqFecSender_WAIT_X_S_ACK(SCHCArqFecSender& ctx);
        ~SCHCArqFecSender_WAIT_X_S_ACK();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        uint32_t                calculate_crc32(const std::vector<uint8_t>& msg); 
        std::vector<uint8_t>    extractTiles(uint8_t firstTileID, uint8_t nTiles);

        SCHCArqFecSender& _ctx;
};
