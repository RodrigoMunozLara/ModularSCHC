#ifndef SCHC_Node_Ack_on_error_hpp
#define SCHC_Node_Ack_on_error_hpp

#include "SCHC_Node_Macros.hpp"
#include "SCHC_Node_State_Machine.hpp"
#include <math.h>
#include "SCHC_Node_Message.hpp"
#include <vector>
#include <deque>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class SCHC_Node_Ack_on_error: public SCHC_Node_State_Machine
{
    public:
        SCHC_Node_Ack_on_error();
        ~SCHC_Node_Ack_on_error();
        uint8_t init(uint8_t ruleID, uint8_t dTag, uint8_t windowSize, uint8_t tileSize, uint8_t n, uint8_t ackMode, SCHC_Node_Stack_L2* stack_ptr, int retTimer, uint8_t ackReqAttempts);
        uint8_t start(char *msg=nullptr, int len=0);
        uint8_t enqueue_message(char* msg, int len);
        uint8_t execute(char *msg=nullptr, int len=0);
    private:
        void        message_reception_loop();
        void        send_ack_req();
        void        send_sender_abort();
        uint8_t     TX_INIT_send_fragments(char *msg, int len);
        uint8_t     TX_SEND_send_fragments();
        uint8_t     TX_WAIT_x_ACK_receive_ack(char *msg, int len);
        uint8_t     TX_END_free_resources();
        uint8_t     TX_RESEND_MISSING_FRAG_send_fragments(char *msg=nullptr, int len=0);
        uint8_t     mtuUpgrade(int mtu);
        uint8_t     divideInTiles(char *buffer, int len);
        uint8_t     extractTiles(uint8_t firstTileID, uint8_t nTiles, char *buff);
        int         getCurrentTile_ptr(int window, int fcn);
        uint8_t     get_current_fcn(int bitmap_ptr); 
        void        printTileArray();
        uint32_t    calculate_crc32(const char *data, size_t length);

        /* Static SCHC parameters */
        uint8_t         _ruleID;
        uint8_t         _dTag;
        uint8_t         _windowSize;
        uint8_t         _nWindows;
        uint8_t         _nFullTiles;    // in tiles
        uint8_t         _lastTileSize;  // in bytes
        uint8_t         _tileSize;      // in bytes
        char**          _tilesArray;
        char*           _lastTile;
        uint8_t         _ackMode;
        int             _retransTimer;
        uint8_t         _maxAckReq;
        uint32_t        _rcs;
        uint8_t**       _bitmapArray;
        int             _retransTimer_counter;
        int             _loop_counter;
        bool            _all_tiles_sent;
        std::vector<uint8_t>    _win_with_errors;

        /* Dynamic SCHC parameters */
        uint8_t     _currentState;
        uint8_t     _currentWindow;
        uint8_t     _currentFcn;
        int         _currentBitmap_ptr;
        int         _currentTile_ptr;
        uint8_t     _last_confirmed_window;
        
        /* Static LoRaWAN parameters*/
        int             _current_L2_MTU;
        SCHC_Node_Stack_L2*  _stack;

        /* Flags */  
        bool            _running;
        bool            _retrans_ack_req_flag;
        bool            _send_schc_ack_req_flag;
};

#endif