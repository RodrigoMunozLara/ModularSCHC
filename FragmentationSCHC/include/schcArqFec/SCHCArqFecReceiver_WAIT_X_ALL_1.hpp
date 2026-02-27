#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include "schcArqFec/SCHCArqFecReceiver.hpp"
#include <spdlog/spdlog.h>
#include <memory>

class SCHCArqFecReceiver;   // ← forward declaration

class SCHCArqFecReceiver_WAIT_X_ALL_1: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCArqFecReceiver_WAIT_X_ALL_1(SCHCArqFecReceiver& ctx);
        ~SCHCArqFecReceiver_WAIT_X_ALL_1();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        int                     get_tile_ptr(uint8_t window, uint8_t fcn);
        int                     get_bitmap_ptr(uint8_t fcn);
        void                    printMatrixHex(const std::vector<std::vector<uint8_t>>& matrix);
        void                    decodeCmatrix();
        void                    storeLastTileinCmatrix(std::vector<uint8_t> lastTile);
        std::vector<uint8_t>    convertDmatrix_to_SCHCPacket();
        uint32_t                calculate_crc32(const std::vector<uint8_t>& msg);

        SCHCArqFecReceiver& _ctx;
};
