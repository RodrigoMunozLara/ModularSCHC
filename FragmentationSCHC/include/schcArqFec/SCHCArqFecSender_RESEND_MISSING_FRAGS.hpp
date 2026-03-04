#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include "spdlog/fmt/bin_to_hex.h"
#include <memory>



class SCHCArqFecSender;   // ← forward declaration

class SCHCArqFecSender_RESEND_MISSING_FRAGS: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCArqFecSender_RESEND_MISSING_FRAGS(SCHCArqFecSender& ctx);
        ~SCHCArqFecSender_RESEND_MISSING_FRAGS();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        std::vector<uint8_t>    extractTiles(uint8_t firstTileID, uint8_t nTiles);
        int                     getCurrentTile_ptr(int window, int bitmap_ptr);
        uint8_t                 get_current_fcn(int bitmap_ptr);

        SCHCArqFecSender& _ctx;
};
