#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include "spdlog/fmt/bin_to_hex.h"
#include <memory>



class SCHCArqFecSender;   // ← forward declaration

class SCHCArqFecSender_SEND: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCArqFecSender_SEND(SCHCArqFecSender& ctx);
        ~SCHCArqFecSender_SEND();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
        void save_time();
        void save_time_ack();
        void save_time_all_1();
    private:
        std::vector<uint8_t>    extractTiles(uint8_t firstTileID, uint8_t nTiles);

        SCHCArqFecSender& _ctx;
};
