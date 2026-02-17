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
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        int                     getCurrentTile_ptr(int window, int bitmap_ptr);
        uint8_t                 get_current_fcn(int bitmap_ptr);
        std::vector<uint8_t>    extractTiles(uint8_t firstTileID, uint8_t nTiles);

        SCHCAckOnErrorSender& _ctx;
};
