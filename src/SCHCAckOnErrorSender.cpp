#include "SCHCAckOnErrorSender.hpp"


SCHCAckOnErrorSender::SCHCAckOnErrorSender(SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore): _dir(dir), _appConfig(appConfig), _schcCore(schcCore)
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
        _retransTimer           = 12 * 60 * 60;
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

    }



}

SCHCAckOnErrorSender::~SCHCAckOnErrorSender()
{
}

void SCHCAckOnErrorSender::start(char* schc_packet, int len)
{
    if(_appConfig.schc.schc_l2_protocol.compare("lorawan") == 0)
    {
        /*
             8.4.3.1. Sender Behavior

            At the beginning of the fragmentation of a new SCHC Packet:
            the fragment sender MUST select a RuleID and DTag value pair 
            for this SCHC Packet. A Rule MUST NOT be selected if the values 
            of M and WINDOW_SIZE for that Rule are such that the SCHC Packet 
            cannot be fragmented in (2^M) * WINDOW_SIZE tiles or less.
            the fragment sender MUST initialize the Attempts counter to 0 for 
            that RuleID and DTag value pair
        */

        if(len > (pow(2,_m)*_windowSize)*_tileSize)
        {
            SPDLOG_ERROR("The message is larger than {} tiles ", (int)pow(2,_m)*_windowSize);
            return;
        }
    }    

    setState(SCHCAckOnErrorSenderStates::STATE_INIT);
    _currentState->execute(schc_packet, len);

}

void SCHCAckOnErrorSender::execute(char* msg, int len)
{
    if (msg !=nullptr)
    {
        _currentState->execute(msg, len);
    }
    else
    {
        _currentState->execute();
    }
}

void SCHCAckOnErrorSender::release()
{
}

void SCHCAckOnErrorSender::setState(SCHCAckOnErrorSenderStates state)
{
    if(state == SCHCAckOnErrorSenderStates::STATE_INIT)
    {
        SPDLOG_INFO("Changing STATE to STATE_INIT");
        _currentState = std::make_unique<SCHCAckOnErrorSender_INIT>(*this);
    }
    else if (state == SCHCAckOnErrorSenderStates::STATE_SEND)
    {
        SPDLOG_INFO("Changing STATE to STATE_SEND");
        _currentState = std::make_unique<SCHCAckOnErrorSender_SEND>(*this);
    }
    else if (state == SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK)
    {
        SPDLOG_INFO("Changing STATE to STATE_WAIT_x_ACK");
        _currentState = std::make_unique<SCHCAckOnErrorSender_WAIT_X_ACK>(*this);
    }
    else if (state == SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG)
    {
        SPDLOG_INFO("Changing STATE to STATE_RESEND_MISSING_FRAG");
        _currentState = std::make_unique<SCHCAckOnErrorSender_RESEND_MISSING_FRAG>(*this);
    }
    else if (state == SCHCAckOnErrorSenderStates::STATE_ERROR)
    {
        SPDLOG_INFO("Changing STATE to STATE_ERROR");
        _currentState = std::make_unique<SCHCAckOnErrorSender_ERROR>(*this);
    }
    else if (state == SCHCAckOnErrorSenderStates::STATE_END)
    {
        SPDLOG_INFO("Changing STATE to STATE_END");
        _currentState = std::make_unique<SCHCAckOnErrorSender_END>(*this);
    }
    
}
