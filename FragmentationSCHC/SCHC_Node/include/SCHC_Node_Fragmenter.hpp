#ifndef SCHC_Node_Fragmenter_hpp
#define SCHC_Node_Fragmenter_hpp

#include "SCHC_Node_Macros.hpp"
#include "SCHC_Node_Session.hpp"
#include "SCHC_Node_LoRaWAN_RAK4631.hpp"
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class SCHC_Node_Fragmenter
{
    public:
        SCHC_Node_Fragmenter();
        uint8_t initialize(uint8_t protocol);
        uint8_t send(char* buffer, int len);
        uint8_t process_received_message(char*  buffer, int len, int fport);
    private:
        int         get_free_session_id(uint8_t direction);
        uint8_t     associate_session_id(int rule_id, int sessionId);
        uint8_t     disassociate_session_id(int rule_id);
        int         get_session_id(int rule_id);
        uint8_t _protocol;
        SCHC_Node_Session                 _uplinkSessionPool[_SESSION_POOL_SIZE];
        SCHC_Node_Session                 _downlinkSessionPool[_SESSION_POOL_SIZE];
        SCHC_Node_Stack_L2*                          _stack;
        std::unordered_map<int, int>            _associationMap;
};

#endif  // SCHC_Node_Fragmenter_hpp