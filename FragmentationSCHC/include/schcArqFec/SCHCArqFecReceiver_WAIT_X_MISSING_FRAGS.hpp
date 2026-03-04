#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <cmath>

class SCHCArqFecReceiver;   // ← forward declaration

class SCHCArqFecReceiver_WAIT_X_MISSING_FRAGS: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCArqFecReceiver_WAIT_X_MISSING_FRAGS(SCHCArqFecReceiver& ctx);
        ~SCHCArqFecReceiver_WAIT_X_MISSING_FRAGS();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        int                     get_tile_ptr(uint8_t window, uint8_t fcn);
        int                     get_bitmap_ptr(uint8_t fcn);
        void                    storeTileinCmatrix(std::vector<uint8_t> tile, int w, int fcn);
        void                    printMatrixHex(const std::vector<std::vector<uint8_t>>& matrix);
        bool                    checkEnoughSymbols();
        void                    decodeCmatrix();
        std::vector<uint8_t>    convertDmatrix_to_SCHCPacket();
        uint32_t                calculate_crc32(const std::vector<uint8_t>& msg);
        uint8_t                 get_c_from_bitmap(uint8_t window);

        SCHCArqFecReceiver& _ctx;
};
