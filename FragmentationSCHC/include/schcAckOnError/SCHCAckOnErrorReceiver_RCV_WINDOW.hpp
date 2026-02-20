#pragma once

#include "SCHCGWMessage.hpp"
#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include "Types.hpp"
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>
#include <sstream>

class SCHCAckOnErrorReceiver;   // ← forward declaration

class SCHCAckOnErrorReceiver_RCV_WINDOW: public ISCHCState
{
    private:
        /* data */
    public:
        SCHCAckOnErrorReceiver_RCV_WINDOW(SCHCAckOnErrorReceiver& ctx);
        ~SCHCAckOnErrorReceiver_RCV_WINDOW();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    
    private:
        int                     get_tile_ptr(uint8_t window, uint8_t fcn);
        int                     get_bitmap_ptr(uint8_t fcn);
        uint8_t                 get_c_from_bitmap(uint8_t window);
        std::vector<uint8_t>    get_bitmap_array_vec(uint8_t window);
        bool                    check_rcs(uint32_t rcs);
        uint32_t                calculate_crc32(const std::vector<uint8_t>& data);
        void                    print_bitmap_array_str();
        std::string             get_bitmap_array_str(uint8_t window);

        SCHCAckOnErrorReceiver& _ctx;
};
