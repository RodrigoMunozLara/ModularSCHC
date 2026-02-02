#pragma once

#include "ISCHCStateMachine.hpp"
#include "ISCHCStack.hpp"
#include "Types.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "SCHCLoRaWANStack.hpp"
#include "SCHCAckOnErrorSender_INIT.hpp"
#include "SCHCAckOnErrorSender_SEND.hpp"
#include "SCHCAckOnErrorSender_WAIT_X_ACK.hpp"
#include "SCHCAckOnErrorSender_RESEND_MISSING_FRAG.hpp"
#include "SCHCAckOnErrorSender_ERROR.hpp"
#include "SCHCAckOnErrorSender_END.hpp"

#include <cmath>
#include <spdlog/spdlog.h>

class SCHCAckOnErrorSender: public ISCHCStateMachine
{
    public:
        SCHCAckOnErrorSender(SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore);
        ~SCHCAckOnErrorSender() override;
        void        start(char* schc_packet, int len) override;
        void        execute(char* msg=nullptr, int len =-1) override;
        void        release() override;
        void        setState(SCHCAckOnErrorSenderStates state) override;
    private:
        SCHCFragDir _dir;
        AppConfig   _appConfig;
        SCHCCore&   _schcCore;
        
        std::unique_ptr<ISCHCState> _currentState;
        ISCHCStack*                 _stack;

    public:
        /* Static SCHC parameters*/
        ProtocolType        _protoType;
        uint8_t             _protocol;
        uint8_t             _direction;
        uint8_t             _ruleID;
        uint8_t             _dTag;
        uint8_t             _tileSize;              // tile size in bytes
        uint8_t             _m;                     // bits of the W field
        uint8_t             _n;                     // bits of the FCN field
        uint8_t             _windowSize;            // tiles in a SCHC window
        uint8_t             _t;                     // bits of the DTag field
        uint8_t             _maxAckReq;             // max number of ACK Request msg
        int                 _retransTimer;          // Retransmission timer in seconds
        int                 _inactivityTimer;       // Inactivity timer in seconds

        /* Dynamic SCHC parameters */
        uint8_t                 _currentWindow;
        uint8_t                 _currentFcn;
        int                     _currentBitmap_ptr;
        int                     _currentTile_ptr;
        uint8_t                 _last_confirmed_window;
        uint8_t                 _nWindows;
        uint8_t                 _nFullTiles;    // in tiles
        uint8_t                 _lastTileSize;  // in bytes
        char**                  _tilesArray;
        char*                   _lastTile;
        uint32_t                _rcs;
        uint8_t**               _bitmapArray;
        int                     _retransTimer_counter;
        bool                    _all_tiles_sent;
        std::vector<uint8_t>    _win_with_errors;
        SCHCAckMechanism        _ackMechanism;
        uint8_t                 _rtxAttemptsCounter;
};