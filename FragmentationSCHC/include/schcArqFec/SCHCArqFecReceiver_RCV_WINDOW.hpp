#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <cmath>

class SCHCArqFecReceiver;   // ← forward declaration

class SCHCArqFecReceiver_RCV_WINDOW: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCArqFecReceiver_RCV_WINDOW(SCHCArqFecReceiver& ctx);
        ~SCHCArqFecReceiver_RCV_WINDOW();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        int     get_tile_ptr(uint8_t window, uint8_t fcn);
        int     get_bitmap_ptr(uint8_t fcn);
        void    storeTileinCmatrix(std::vector<uint8_t> tile, int w, int fcn);
        void    printMatrixHex(const std::vector<std::vector<uint8_t>>& matrix);
        bool    checkEnoughSymbols();

        SCHCArqFecReceiver& _ctx;
};
