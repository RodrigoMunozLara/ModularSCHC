#pragma once

#include "interfaces/ISCHCStateMachine.hpp"
#include "interfaces/ISCHCStack.hpp"
#include "Types.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "SCHCTimer.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver_INIT.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver_RCV_WINDOW.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver_WAIT_END.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver_END.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver_ERROR.hpp"

#include <cmath>
#include <spdlog/spdlog.h>

class SCHCSession;

class SCHCAckOnErrorReceiver: public ISCHCStateMachine
{
    public:
        SCHCAckOnErrorReceiver(SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore, SCHCSession& schcSession);
        ~SCHCAckOnErrorReceiver() override;
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
        
        std::unique_ptr<ISCHCState>     _currentState;
        ISCHCStack*                     _stack;
        SCHCTimer                       _timer;
        SCHCAckOnErrorReceiverStates    _currentStateStr;
        SCHCAckOnErrorReceiverStates    _nextStateStr;


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
        int                 _inactivityTimer;       // Inactivity timer in seconds
        int                 _nMaxWindows;           // In LoRaWAN: 4
        int                 _nTotalTiles;

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
        std::vector<uint8_t>                _last_ack_sent_buffer;
        int                                 _last_ack_sent_c;
        int                                 _last_ack_sent_w;


        
        std::string                         _dev_id;
        int                                 _last_window;   // stores the number of the last window
        uint8_t                             _error_prob;

        bool                                _enable_to_process_msg; // Este flag se usa para descartar cualquier mensajes remanente de otras sesiones que intente arrancar la sesion


        int                                 _counter;





};