#pragma once

#include "interfaces/ISCHCStateMachine.hpp"
#include "interfaces/ISCHCStack.hpp"
#include "Types.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "stacks/SCHCLoRaWANStack.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender_INIT.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender_SEND.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender_WAIT_X_ACK.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender_RESEND_MISSING_FRAG.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender_ERROR.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender_END.hpp"
#include "SCHCTimer.hpp"

#include <cmath>
#include <spdlog/spdlog.h>

class SCHCSession;

class SCHCAckOnErrorSender: public ISCHCStateMachine
{
    public:
        SCHCAckOnErrorSender(SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore, SCHCSession& schcSession);
        ~SCHCAckOnErrorSender() override;
        void        execute(const std::vector<uint8_t>& msg = {}) override;
        void        timerExpired() override;
        void        release() override;
        void        executeAgain() override;
        void        executeTimer(int delay) override;
        void        enqueueTimer() override;

        SCHCFragDir     _dir;
        AppConfig       _appConfig;
        SCHCCore&       _schcCore;
        SCHCSession&    _schcSession;
        
        std::unique_ptr<ISCHCState> _currentState;
        ISCHCStack*                 _stack;
        SCHCTimer                   _timer;
        SCHCAckOnErrorSenderStates  _currentStateStr;
        SCHCAckOnErrorSenderStates  _nextStateStr;


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

        /* Dynamic SCHC parameters */
        uint8_t                             _currentWindow;
        uint8_t                             _currentFcn;
        int                                 _currentBitmap_ptr;
        int                                 _currentTile_ptr;
        uint8_t                             _last_confirmed_window;
        uint8_t                             _nWindows;
        uint8_t                             _nFullTiles;    // in tiles
        uint8_t                             _lastTileSize;  // in bytes
        std::vector<std::vector<uint8_t>>   _tilesArray;
        std::vector<uint8_t>                _lastTile;
        uint32_t                            _rcs;
        std::vector<std::vector<uint8_t>>   _bitmapArray;
        bool                                _all_tiles_sent;
        std::vector<uint8_t>                _win_with_errors;
        SCHCAckMechanism                    _ackMechanism;
        uint8_t                             _rtxAttemptsCounter;
        uint32_t                            _current_L2_MTU;

        bool                                _send_schc_ack_req_flag;
};