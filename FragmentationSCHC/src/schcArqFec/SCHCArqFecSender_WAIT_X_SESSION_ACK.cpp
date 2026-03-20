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
        SCHCMsgType msg_type = decoder.get_msg_type(_ctx._protoType, _ctx._ruleID, msg);

        if(msg_type == SCHCMsgType::SCHC_ACK_MSG)
        {
            SPDLOG_DEBUG("Receiving a SCHC ACK");

            decoder.decodeMsg(_ctx._protoType, _ctx._ruleID, msg, SCHCAckMechanism::ARQ_FEC, &(_ctx._bitmapArray));
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
                return;
                
            }
            else
            {
                SPDLOG_ERROR("The SCHC ACK message must have the following parameters (C=1 and W=3)");    
                return; 
            }
        }
        else if (msg_type == SCHCMsgType::SCHC_COMPOUND_ACK)
        {
            SPDLOG_DEBUG("Receiving a SCHC Compound ACK");

            SPDLOG_DEBUG("Stoping the Retransmission timer...");
            _ctx._timer.stop();

            decoder.decodeMsg(_ctx._protoType, _ctx._ruleID, msg, SCHCAckMechanism::ACK_COMPOUND, &(_ctx._bitmapArray));
            uint8_t c               = decoder.get_c();
            _ctx._win_with_errors   = decoder.get_w_vector();

            decoder.print_msg(SCHCMsgType::SCHC_COMPOUND_ACK, msg, _ctx._bitmapArray);
            SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
            _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_RESEND_MISSING_TILES;
            _ctx.executeAgain();
            return;

        }
        else
        {
            SPDLOG_WARN("Only SCHC ACK are permitted");
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

    /* Crea un mensaje SCHC en formato hexadecimal */
    std::vector<uint8_t> schc_all_1_message = encoder.create_all_1_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._rcs, _ctx._lastTile);

    /* Imprime los mensajes para visualizacion ordenada */
    encoder.print_msg(SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message); 

    /* Envía el mensaje a la capa 2*/
    _ctx._stack->send_frame(_ctx._ruleID, schc_all_1_message);

    SPDLOG_DEBUG("Setting S-timer: {} seconds", _ctx._sTimer);
    _ctx.executeTimer(_ctx._sTimer);

    _ctx.executeAgain();
}

void SCHCArqFecSender_WAIT_X_SESSION_ACK::release()
{
}

