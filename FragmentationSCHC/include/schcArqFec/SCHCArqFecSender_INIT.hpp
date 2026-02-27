#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include "spdlog/fmt/bin_to_hex.h"
#include <memory>



class SCHCArqFecSender;   // ← forward declaration

class SCHCArqFecSender_INIT: public ISCHCState
{
    public:
        SCHCArqFecSender_INIT(SCHCArqFecSender& ctx);
        ~SCHCArqFecSender_INIT();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        void                    divideInTiles(const std::vector<uint8_t>& msg);
        uint32_t                calculate_crc32(const std::vector<uint8_t>& msg); 
        void                    generateDataMatrix(const std::vector<uint8_t>& inputBuffer); 
        bool                    generateEncodedMatrix(const std::vector<std::vector<uint8_t>>& matrixD);
        std::vector<uint8_t>    generateencodedSCHCpacket(const std::vector<std::vector<uint8_t>>& matrix);
        void                    printMatrixHex(const std::vector<std::vector<uint8_t>>& matrix);
        std::vector<uint8_t>    extractTiles(uint8_t firstTileID, uint8_t nTiles);
        std::vector<uint8_t>    packBitsWithPadding(const std::vector<uint8_t>& bits);

        SCHCArqFecSender& _ctx;
};
