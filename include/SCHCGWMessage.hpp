#pragma once

#include "Types.hpp"

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <vector>
#include <spdlog/spdlog.h>


class SCHCGWMessage
{
    public:
        SCHCGWMessage();
        std::vector<uint8_t>    create_schc_ack(uint8_t rule_id, uint8_t dtag, uint8_t w, uint8_t c, const std::vector<uint8_t> bitmap_vector, bool must_compress = true);
        std::vector<uint8_t>    create_schc_ack_compound(uint8_t rule_id, uint8_t dtag, int last_win, const std::vector<uint8_t> c_vector, const std::vector<std::vector<uint8_t>> bitmap_array, uint8_t win_size);
        SCHCMsgType             get_msg_type(ProtocolType protocol, uint8_t rule_id, std::vector<uint8_t> msg);
        uint8_t                 decode_message(ProtocolType protocol, uint8_t rule_id, std::vector<uint8_t> msg);
        uint8_t                 get_w();
        uint8_t                 get_fcn();
        uint8_t                 get_dtag();
        int                     get_schc_payload_len();
        std::vector<uint8_t>    get_schc_payload();
        uint32_t                get_rcs();
        std::string             get_compound_bitmap_str();
        void                    printMsg(ProtocolType protocol, SCHCMsgType msgType, std::vector<uint8_t> msg);
    private:
        SCHCMsgType             _msg_type;
        uint8_t                 _w;
        uint8_t                 _fcn;
        uint8_t                 _dtag;
        int                     _schc_payload_len;  // in bits
        std::vector<uint8_t>    _schc_payload;
        uint32_t                _rcs;
        std::string             _compound_ack_string;
};
