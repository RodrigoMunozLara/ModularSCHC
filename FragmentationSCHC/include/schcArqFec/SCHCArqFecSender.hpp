#pragma once

#include "interfaces/ISCHCStateMachine.hpp"
#include "interfaces/ISCHCStack.hpp"
#include "Types.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "stacks/SCHCLoRaWANStack.hpp"
#include "schcArqFec/SCHCArqFecSender_INIT.hpp"
#include "schcArqFec/SCHCArqFecSender_WAIT_X_S_ACK.hpp"
#include "schcArqFec/SCHCArqFecSender_SEND.hpp"
#include "schcArqFec/SCHCArqFecSender_WAIT_X_SESSION_ACK.hpp"
#include "schcArqFec/SCHCArqFecSender_END.hpp"
#include "schcAckOnError/SCHCNodeMessage.hpp"

#include "SCHCTimer.hpp"

#include <cmath>
#include <spdlog/spdlog.h>

class SCHCSession;

class SCHCArqFecSender: public ISCHCStateMachine
{
    public:
        SCHCArqFecSender(SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore, SCHCSession& schcSession);
        ~SCHCArqFecSender() override;
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
        SCHCArqFecSenderStates      _currentStateStr;
        SCHCArqFecSenderStates      _nextStateStr;


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
        int                 _sTimer;                // S timer in seconds

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




        /* ARQ FEC variables */
        static constexpr std::size_t        _ksymbols = 10;                     // number of data symbols for each row
        static constexpr std::size_t        _nsymbols = 15;                     // number of encoded symbols for each row
        static constexpr std::size_t        _mbits = 8;                     // number of bits for each symbol
        std::vector<std::vector<uint8_t>>   _dataMatrix;                // Matrix D (S rows x k columns)
        std::vector<std::vector<uint8_t>>   _encodedMatrix;             // Matrix C (S rows x n columns)
        std::vector<uint8_t>                _residualBitsContainer;     // Individual bits (0 or 1)
        int                                 _residualCodingBitsCount;   // (P mod (k * m)) as integer
        int                                 _rowCount;                  // S = floor(P / (k * m)) as integer

        std::vector<uint8_t>                _first_fragment_msg;
};