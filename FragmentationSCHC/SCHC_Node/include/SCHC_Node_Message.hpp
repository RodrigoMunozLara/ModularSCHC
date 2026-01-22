#ifndef SCHC_Node_Message_hpp
#define SCHC_Node_Message_hpp

#include "SCHC_Node_Macros.hpp"
#include <vector>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class SCHC_Node_Message
{
    public:
        SCHC_Node_Message();
        int                     create_regular_fragment(uint8_t ruleID, uint8_t dtag, uint8_t w, uint8_t fcn, char *payload, int payload_len, char *buffer);
        int                     create_ack_request(uint8_t ruleID, uint8_t dtag, uint8_t w, char *buffer);
        int                     create_sender_abort(uint8_t ruleID, uint8_t dtag, uint8_t w, char *buffer);
        int                     create_all_1_fragment(uint8_t ruleID, uint8_t dtag, uint8_t w, uint32_t rcs, char *payload, int payload_len, char *buffer);
        uint8_t                 get_msg_type(uint8_t protocol, int rule_id, char *msg, int len);
        uint8_t                 decodeMsg(uint8_t protocol, int rule_id, char *msg, int len, uint8_t** bitmapArray = nullptr);
        void                    print_msg(uint8_t msgType, char *msg, int len, uint8_t** bitmapArray = nullptr);
        void                    printBin(uint8_t val);
        uint8_t                 get_c();
        uint8_t                 get_w();
        std::vector<uint8_t>    get_w_vector();
    private:
        uint8_t                 _msg_type;
        uint8_t                 _w;
        uint8_t                 _c;
        uint8_t                 _fcn;
        uint8_t                 _dtag;
        int                     _schc_payload_len;
        char*                   _schc_payload = nullptr;
        char*                   _rcs = nullptr;
        char*                   _bitmap = nullptr; 
        std::vector<uint8_t>    _windows_with_error;
};

#endif