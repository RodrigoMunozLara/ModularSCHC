#ifndef SCHC_Node_Session_hpp
#define SCHC_Node_Session_hpp

#include "SCHC_Node_Macros.hpp"
#include "SCHC_Node_Ack_on_error.hpp"
#include "SCHC_Node_State_Machine.hpp"
#include "SCHC_Node_Stack_L2.hpp"
#include <vector>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>


class SCHC_Node_Session
{
    public:
        SCHC_Node_Session();
        uint8_t initialize(uint8_t protocol, uint8_t direction, uint8_t dTag, SCHC_Node_Stack_L2* stack_ptr);
        uint8_t startFragmentation(char *buffer, int len);
        bool    getIsUsed();
        void    setIsUsed(bool isUsed);
        void    process_message(char* msg, int len);
    private:
        uint8_t createStateMachine();
        uint8_t destroyStateMachine();
        bool _isUsed;
        uint8_t _protocol;
        uint8_t _direction;
        uint8_t _ruleID;
        uint8_t _dTag;
        uint8_t _tileSize;              // tile size in bytes
        uint8_t _m;                     // bits of the W field
        uint8_t _n;                     // bits of the FCN field
        uint8_t _windowSize;            // tiles in a SCHC window
        uint8_t _t;                     // bits of the DTag field
        uint8_t _maxAckReq;             // max number of ACK Request msg
        int _retransTimer;              // Retransmission timer in seconds
        int _inactivityTimer;           // Inactivity timer in seconds
        uint8_t _txAttemptsCounter;     // transmission attempt counter
        uint8_t _rxAttemptsCounter;     // reception attempt counter
        int _maxMsgSize;                // Maximum size of a SCHC packet in bytes
        SCHC_Node_State_Machine*     _stateMachine;
        SCHC_Node_Stack_L2*          _stack;
        
};

#endif
