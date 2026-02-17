#pragma once

#include "Types.hpp"

#include <spdlog/spdlog.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstdio>


class SCHCNodeMessage
{
    public:
        SCHCNodeMessage();
        ~SCHCNodeMessage();
        std::vector<uint8_t>    create_regular_fragment(uint8_t ruleID, uint8_t dtag, uint8_t w, uint8_t fcn, const std::vector<uint8_t>& payload);
        std::vector<uint8_t>    create_ack_request(uint8_t ruleID, uint8_t dtag, uint8_t w);
        std::vector<uint8_t>    create_sender_abort(uint8_t ruleID, uint8_t dtag, uint8_t w);
        std::vector<uint8_t>    create_all_1_fragment(uint8_t ruleID, uint8_t dtag, uint8_t w, uint32_t rcs, const std::vector<uint8_t>& payload);
        SCHCMsgType             get_msg_type(ProtocolType protocol, int rule_id, const std::vector<uint8_t>& msg);
        uint8_t                 decodeMsg(ProtocolType protocol, int rule_id, const std::vector<uint8_t>& msg, SCHCAckMechanism ack_type, std::vector<std::vector<uint8_t>>* bitmap_array = nullptr);
        void                    print_msg(SCHCMsgType msgType, const std::vector<uint8_t>& msg, const std::vector<std::vector<uint8_t>>& bitmap_array = {});
        void                    printBin(uint8_t val);
        uint8_t                 get_c();
        uint8_t                 get_w();
        std::vector<uint8_t>    get_w_vector();
        std::vector<uint8_t>    get_schc_payload();
    private:
        SCHCMsgType             _msg_type;
        SCHCAckMechanism        _ack_type;
        uint8_t                 _w;
        uint8_t                 _c;
        uint8_t                 _fcn;
        uint8_t                 _dtag;
        int                     _schc_payload_len;
        std::vector<uint8_t>    _schc_payload;
        std::vector<uint8_t>    _rcs;
        std::vector<uint8_t>    _bitmap; 
        std::vector<uint8_t>    _windows_with_error;
};
