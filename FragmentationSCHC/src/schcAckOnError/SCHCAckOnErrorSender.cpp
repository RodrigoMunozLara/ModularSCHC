#include "schcAckOnError/SCHCAckOnErrorSender.hpp"
#include "SCHCSession.hpp"


SCHCAckOnErrorSender::SCHCAckOnErrorSender(SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore, SCHCSession& schcSession): _dir(dir), _appConfig(appConfig), _schcCore(schcCore), _schcSession(schcSession)
{
    _stack = _schcCore._stack.get();

    if(appConfig.schc.schc_l2_protocol.compare("lorawan") == 0)
    {
        /* Static SCHC parameters */
        _protoType              = ProtocolType::LORAWAN;
        _ruleID                 = 20;
        _dTag                   = -1;
        _windowSize             = 63;
        _tileSize               = 10;
        _retransTimer           = 5; /* seconds */
        _maxAckReq              = 8;
        _m                      = 2;

        if(appConfig.schc.schc_ack_mechanism.compare("ack_end_win") == 0) _ackMechanism = SCHCAckMechanism::ACK_END_WIN;
        else if (appConfig.schc.schc_ack_mechanism.compare("ack_end_session") == 0) _ackMechanism = SCHCAckMechanism::ACK_END_SES;
        else if (appConfig.schc.schc_ack_mechanism.compare("compound_ack") == 0) _ackMechanism = SCHCAckMechanism::ACK_COMPOUND;
         
        /* Dynamic SCHC parameters */
        _nFullTiles             = 0;
        _lastTileSize           = 0;             
        _rtxAttemptsCounter     = 0;
        _all_tiles_sent         = false;
        _last_confirmed_window  = -1;
        _current_L2_MTU         = _schcCore._stack->getMtu();
        SPDLOG_DEBUG("Using MTU: {}", _current_L2_MTU);

        SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in false");
        _send_schc_ack_req_flag = false;

        SPDLOG_DEBUG("Changing STATE to STATE_INIT");
        _currentState = std::make_unique<SCHCAckOnErrorSender_INIT>(*this);

    }
}

SCHCAckOnErrorSender::~SCHCAckOnErrorSender()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorSender destructor()");
}

void SCHCAckOnErrorSender::execute(const std::vector<uint8_t>& msg)
{
    if (msg.empty()) 
    {
        SPDLOG_DEBUG("Calling the execute() method of the current state");
        _currentState->execute();
    } 
    else 
    {
        SPDLOG_DEBUG("Calling the execute(msg) method of the state machine");
        _currentState->execute(msg);
    }

    if(_currentStateStr != _nextStateStr)
    {
        if(_nextStateStr == SCHCAckOnErrorSenderStates::STATE_INIT)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_INIT");
            _currentState = std::make_unique<SCHCAckOnErrorSender_INIT>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_INIT;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_SEND)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_SEND");
            _currentState = std::make_unique<SCHCAckOnErrorSender_SEND>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_SEND;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_WAIT_x_ACK");
            _currentState = std::make_unique<SCHCAckOnErrorSender_WAIT_X_ACK>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_RESEND_MISSING_FRAG");
            _currentState = std::make_unique<SCHCAckOnErrorSender_RESEND_MISSING_FRAG>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_END)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_END");
            _currentState = std::make_unique<SCHCAckOnErrorSender_END>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_END;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_ERROR)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_ERROR");
            _currentState = std::make_unique<SCHCAckOnErrorSender_ERROR>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_ERROR;
        }
        
    }

}

void SCHCAckOnErrorSender::timerExpired()
{
    _currentState->timerExpired();

    if(_currentStateStr != _nextStateStr)
    {
        if(_nextStateStr == SCHCAckOnErrorSenderStates::STATE_INIT)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_INIT");
            _currentState = std::make_unique<SCHCAckOnErrorSender_INIT>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_INIT;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_SEND)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_SEND");
            _currentState = std::make_unique<SCHCAckOnErrorSender_SEND>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_SEND;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_WAIT_x_ACK");
            _currentState = std::make_unique<SCHCAckOnErrorSender_WAIT_X_ACK>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_RESEND_MISSING_FRAG");
            _currentState = std::make_unique<SCHCAckOnErrorSender_RESEND_MISSING_FRAG>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_END)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_END");
            _currentState = std::make_unique<SCHCAckOnErrorSender_END>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_END;
        }
        else if (_nextStateStr == SCHCAckOnErrorSenderStates::STATE_ERROR)
        {
            SPDLOG_DEBUG("Changing STATE to STATE_ERROR");
            _currentState = std::make_unique<SCHCAckOnErrorSender_ERROR>(*this);
            _currentStateStr = SCHCAckOnErrorSenderStates::STATE_ERROR;
        }
        
    }

}

void SCHCAckOnErrorSender::release()
{
    SPDLOG_DEBUG("Releasing the memory of the current state");
    _currentState.reset();
}

void SCHCAckOnErrorSender::executeAgain()
{
    SPDLOG_DEBUG("Enqueueing an ExecuteAgain event");
    auto evMsg = std::make_unique<EventMessage>();
    evMsg->evType = EventType::ExecuteAgain;
    std::vector<uint8_t> v;
    evMsg->payload = v;
    _schcSession.enqueueEvent(std::move(evMsg));
}

void SCHCAckOnErrorSender::executeTimer(int delay)
{
    SPDLOG_DEBUG("Setting Timer to activate in {} seconds", delay);
    _timer.start(std::chrono::seconds(delay), [&]() { enqueueTimer(); });

}

void SCHCAckOnErrorSender::enqueueTimer() 
{
    SPDLOG_DEBUG("Enqueueing an TimerExpired event");
    auto evMsg = std::make_unique<EventMessage>();
    evMsg->evType = EventType::TimerExpired;
    std::vector<uint8_t> v;
    evMsg->payload = v;
    _schcSession.enqueueEvent(std::move(evMsg));
}
