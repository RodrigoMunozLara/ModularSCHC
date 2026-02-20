#include "SCHCAckOnErrorReceiver.hpp"
#include "SCHCSession.hpp"

SCHCAckOnErrorReceiver::SCHCAckOnErrorReceiver(SCHCFragDir dir, AppConfig &appConfig, SCHCCore &schcCore, SCHCSession &schcSession): _dir(dir), _appConfig(appConfig), _schcCore(schcCore), _schcSession(schcSession)
{
    _stack = _schcCore._stack.get();

    if(appConfig.schc.schc_l2_protocol.compare("lorawan_ns_mqtt") == 0)
    {
        /* Static SCHC parameters */
        _protoType              = ProtocolType::LORAWAN_NS;
        _ruleID                 = 20;
        _dTag                   = -1;
        _windowSize             = 63;
        _tileSize               = 10;
        _inactivityTimer        = 5;    /* seconds */
        _maxAckReq              = 8;
        _m                      = 2;
        _nTotalTiles            = _windowSize * pow(2,_m);

        if(appConfig.schc.schc_ack_mechanism.compare("ack_end_win") == 0) _ackMechanism = SCHCAckMechanism::ACK_END_WIN;
        else if (appConfig.schc.schc_ack_mechanism.compare("ack_end_session") == 0) _ackMechanism = SCHCAckMechanism::ACK_END_SES;
        else if (appConfig.schc.schc_ack_mechanism.compare("compound_ack") == 0) _ackMechanism = SCHCAckMechanism::ACK_COMPOUND;
         
        /* Dynamic SCHC parameters */
        _nFullTiles             = 0;
        _lastTileSize           = 0;             
        _rtxAttemptsCounter     = 0;
        _all_tiles_sent         = false;
        _last_confirmed_window  = 0;
        _current_L2_MTU         = _schcCore._stack->getMtu();
        SPDLOG_DEBUG("Using MTU: {}", _current_L2_MTU);

        SPDLOG_DEBUG("Changing STATE to STATE_RCV_WINDOW");
        _currentState = std::make_unique<SCHCAckOnErrorReceiver_RCV_WINDOW>(*this);

        _enable_to_process_msg = false;

        _counter = 1;

    }
    
}

SCHCAckOnErrorReceiver::~SCHCAckOnErrorReceiver()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorReceiver destructor()");
}

void SCHCAckOnErrorReceiver::execute(const std::vector<uint8_t>& msg)
{
    _dev_id = _schcSession.getDevId();

    if (msg.empty()) 
    {
        _currentState->execute();
    } else 
    {
        _currentState->execute(msg);
    }


    if(_currentStateStr != _nextStateStr)
    {
        if(_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_INIT)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_INIT");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_INIT>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_INIT;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_RCV_WINDOW)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_RCV_WINDOW");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_RCV_WINDOW>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_RCV_WINDOW;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_WAIT_X_MISSING_FRAG");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_WAIT_END)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_WAIT_END");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_WAIT_END>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_END;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_END)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_END");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_END>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_ERROR)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_ERROR");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_ERROR>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_ERROR;
        }
        
    }
}

void SCHCAckOnErrorReceiver::timerExpired()
{
    _currentState->timerExpired();

    if(_currentStateStr != _nextStateStr)
    {
        if(_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_INIT)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_INIT");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_INIT>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_INIT;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_RCV_WINDOW)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_RCV_WINDOW");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_RCV_WINDOW>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_RCV_WINDOW;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_WAIT_X_MISSING_FRAG");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_WAIT_END)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_WAIT_END");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_WAIT_END>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_END;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_END)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_END");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_END>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
        }
        else if (_nextStateStr == SCHCAckOnErrorReceiverStates::STATE_ERROR)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_ERROR");
            _currentState = std::make_unique<SCHCAckOnErrorReceiver_ERROR>(*this);
            _currentStateStr = SCHCAckOnErrorReceiverStates::STATE_ERROR;
        }
        
    }
}

void SCHCAckOnErrorReceiver::release()
{
    SPDLOG_DEBUG("Releasing the memory of the current state");
    _currentState.reset();
}

void SCHCAckOnErrorReceiver::executeAgain()
{
    SPDLOG_DEBUG("Enqueueing an ExecuteAgain event");
    auto evMsg = std::make_unique<EventMessage>();
    evMsg->evType = EventType::ExecuteAgain;
    std::vector<uint8_t> v;
    evMsg->payload = v;
    _schcSession.enqueueEvent(std::move(evMsg));
}

void SCHCAckOnErrorReceiver::executeTimer()
{
    _timer.start(std::chrono::seconds(_inactivityTimer), [&]() { enqueueTimer(); });
}

void SCHCAckOnErrorReceiver::enqueueTimer()
{
    SPDLOG_DEBUG("Enqueueing an TimerExpired event");
    auto evMsg = std::make_unique<EventMessage>();
    evMsg->evType = EventType::TimerExpired;
    std::vector<uint8_t> v;
    evMsg->payload = v;
    _schcSession.enqueueEvent(std::move(evMsg));
}
