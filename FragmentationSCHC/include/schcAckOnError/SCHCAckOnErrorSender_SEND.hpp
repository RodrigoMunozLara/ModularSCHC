#pragma once

#include "schcAckOnError/SCHCNodeMessage.hpp"
#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <memory>
#include <spdlog/spdlog.h>

class SCHCAckOnErrorSender;   // ← forward declaration

class SCHCAckOnErrorSender_SEND: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorSender_SEND(SCHCAckOnErrorSender& ctx);
        ~SCHCAckOnErrorSender_SEND();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        std::vector<uint8_t> extractTiles(uint8_t firstTileID, uint8_t nTiles);
        SCHCAckOnErrorSender& _ctx;
};
