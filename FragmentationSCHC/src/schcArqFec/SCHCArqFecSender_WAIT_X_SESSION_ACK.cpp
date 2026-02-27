#include "schcArqFec/SCHCArqFecSender_WAIT_X_SESSION_ACK.hpp"

SCHCArqFecSender_WAIT_X_SESSION_ACK::SCHCArqFecSender_WAIT_X_SESSION_ACK(SCHCArqFecSender& ctx): _ctx(ctx)
{

}

SCHCArqFecSender_WAIT_X_SESSION_ACK::~SCHCArqFecSender_WAIT_X_SESSION_ACK()
{
    SPDLOG_DEBUG("Executing SCHCArqFecSender_WAIT_X_SESSION_ACK destructor()");
}

void SCHCArqFecSender_WAIT_X_SESSION_ACK::execute(const std::vector<uint8_t>& msg)
{
    if(!msg.empty())
    {
        SCHCNodeMessage decoder;
        SPDLOG_DEBUG("Decoding Message...");
        SCHCMsgType msg_type = decoder.get_msg_type(ProtocolType::LORAWAN, _ctx._ruleID, msg);

        if(msg_type == SCHCMsgType::SCHC_ACK_MSG)
        {
            SPDLOG_DEBUG("Receiving a SCHC ACK msg");

            decoder.decodeMsg(ProtocolType::LORAWAN, _ctx._ruleID, msg, SCHCAckMechanism::ARQ_FEC, &(_ctx._bitmapArray));
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);

            if(c == 1 && w == 3)
            {
                SPDLOG_DEBUG("Stoping the Rtx All-1 timer...");
                _ctx._timer.stop();

                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_SESSION_ACK --> STATE_TX_END");
                _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_END;
                _ctx.executeAgain();
                
            }
            else
            {
                SPDLOG_ERROR("The SCHC ACK message must have the following parameters (C=1 and W=3)");     
            }
        }
        else
        {
            SPDLOG_WARN("Only SCHC ACK are permitted.");
        }

    }
    else
    {
        SPDLOG_WARN("It is not permitted to call the execute method without a message");
    }

}

void SCHCArqFecSender_WAIT_X_SESSION_ACK::timerExpired()
{
    SCHCNodeMessage encoder;        // encoder 

    /* Imprime los mensajes para visualizacion ordenada */
    encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, _ctx._first_fragment_msg);

    /* Envía el mensaje a la capa 2*/
    _ctx._stack->send_frame(_ctx._ruleID, _ctx._first_fragment_msg);

    SPDLOG_DEBUG("Setting S-timer: {} seconds", _ctx._sTimer);
    _ctx.executeTimer(_ctx._sTimer);

    _ctx.executeAgain();
}

void SCHCArqFecSender_WAIT_X_SESSION_ACK::release()
{
}

