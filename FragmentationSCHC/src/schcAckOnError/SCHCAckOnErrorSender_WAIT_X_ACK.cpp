#include "schcAckOnError/SCHCAckOnErrorSender_WAIT_X_ACK.hpp"


SCHCAckOnErrorSender_WAIT_X_ACK::SCHCAckOnErrorSender_WAIT_X_ACK(SCHCAckOnErrorSender& ctx) : _ctx(ctx)
{
}

SCHCAckOnErrorSender_WAIT_X_ACK::~SCHCAckOnErrorSender_WAIT_X_ACK()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorSender_WAIT_X_ACK destructor()");
}

void SCHCAckOnErrorSender_WAIT_X_ACK::execute(const std::vector<uint8_t>& msg)
{
    SCHCNodeMessage decoder;
    SPDLOG_DEBUG("Decoding Message...");
    SCHCMsgType msg_type = decoder.get_msg_type(ProtocolType::LORAWAN, _ctx._ruleID, msg);

    if(msg_type == SCHCMsgType::SCHC_ACK_MSG)
    {
        

        SPDLOG_DEBUG("Receiving a SCHC ACK msg");

        SPDLOG_DEBUG("Stoping the Retransmission timer...");
        _ctx._timer.stop();

        //_retrans_ack_req_flag = false;

        if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_WIN)
        {
            decoder.decodeMsg(ProtocolType::LORAWAN, _ctx._ruleID, msg, SCHCAckMechanism::ACK_END_WIN, &(_ctx._bitmapArray));
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            
            if(c == 1 && w == (_ctx._nWindows-1))
            {
                /* SCHC ACK incluye bitmap sin errores y es un ACK a la ultima ventana*/
                decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);

                _ctx._currentWindow      = _ctx._currentWindow + 1; 

                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_END");
                _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_END;
                _ctx.executeAgain();

            }
            else if(c == 1 && w != (_ctx._nWindows-1))
            {
                /* SCHC ACK incluye bitmap sin errores y NO es un ACK para la ultima ventana*/
                decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);

                _ctx._currentWindow      = _ctx._currentWindow + 1; 
                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_SEND");
                _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_SEND;
                _ctx.executeAgain();
                
            }
            else if(c == 0)
            {
                decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);

                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
                _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG;
                _ctx.executeAgain();
                
            }
        }
        else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_SES)
        {
            decoder.decodeMsg(ProtocolType::LORAWAN, _ctx._ruleID, msg, SCHCAckMechanism::ACK_END_SES, &(_ctx._bitmapArray));
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            _ctx._last_confirmed_window = w;

            if(c == 1)
            {
                /* SCHC ACK incluye bitmap sin errores y es un ACK a la ultima ventana*/
                decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);
                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_END");
                _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_END;
                _ctx.executeAgain();
                
            }
            else
            {
                decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);
                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
                _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG;
                _ctx.executeAgain();
                
            }
        }
        else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_COMPOUND)
        {
            decoder.decodeMsg(ProtocolType::LORAWAN, _ctx._ruleID, msg, SCHCAckMechanism::ACK_COMPOUND, &(_ctx._bitmapArray));
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            _ctx._win_with_errors.push_back(w);

            if(c == 1)
            {
                /* SCHC ACK incluye bitmap sin errores y es un ACK a la ultima ventana*/
                decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);
                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_END");
                _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_END;
                _ctx.executeAgain();
            }
            else
            {
                decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);
                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
                _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG;
                _ctx.executeAgain();
            }
        }

    
    }
    else if(msg_type == SCHCMsgType::SCHC_COMPOUND_ACK)
    {

        SPDLOG_DEBUG("Receiving a SCHC Compound ACK msg");

        SPDLOG_DEBUG("Stoping the Retransmission timer...");
        _ctx._timer.stop();

        decoder.decodeMsg(ProtocolType::LORAWAN, _ctx._ruleID, msg, SCHCAckMechanism::ACK_COMPOUND, &(_ctx._bitmapArray));
        uint8_t c               = decoder.get_c();
        _ctx._win_with_errors   = decoder.get_w_vector();

        if(c == 1)
        {
            /* SCHC ACK incluye bitmap sin errores y es un ACK a la ultima ventana*/

            decoder.print_msg(SCHCMsgType::SCHC_COMPOUND_ACK, msg, _ctx._bitmapArray);
            SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_END");
            _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_END;
            _ctx.executeAgain();
        }
        else
        {
            decoder.print_msg(SCHCMsgType::SCHC_COMPOUND_ACK, msg, _ctx._bitmapArray);
            SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
            _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_RESEND_MISSING_FRAG;
            _ctx.executeAgain();
        }

    }
    else if(msg_type == SCHCMsgType::SCHC_RECEIVER_ABORT_MSG)
    {

            SPDLOG_DEBUG("Receiving a SCHC Receiver-Abort message. Releasing resources...");

            // TODO: Liberar maquinas de estados y recursos de memoria. Se debe retornar la funcion SCHC_Node_Fragmenter::send()
    }

}

void SCHCAckOnErrorSender_WAIT_X_ACK::timerExpired()
{
    if(_ctx._rtxAttemptsCounter <= _ctx._maxAckReq)
    {
        SPDLOG_DEBUG("Preparing SCHC ACK REQ");
        /* ******************* SCHC ACK REQ ********************************* */
        /* Se envía un SCHC ACK REQ para empujar el envio en el downlink
        del SCHC ACK enviado por el SCHC Gateway */
        SCHCNodeMessage encoder_2;
        auto schc_ack_req_msg       = encoder_2.create_ack_request(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow);

        /* Imprime los mensajes para visualizacion ordenada */
        encoder_2.print_msg(SCHCMsgType::SCHC_ACK_REQ_MSG, schc_ack_req_msg);

        /* Envía el mensaje a la capa 2*/
        _ctx._stack->send_frame(_ctx._ruleID, schc_ack_req_msg);

        _ctx._rtxAttemptsCounter++;

        _ctx.executeTimer(_ctx._retransTimer);

         return;
    }
    else
    {
        SPDLOG_DEBUG("The maximum number of SCHC ACK REQ retransmissions has been reached.");
        SPDLOG_DEBUG("Changing STATE: From STATE_TX_SEND --> STATE_TX_ERROR");
        _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_ERROR;
        _ctx.executeAgain();
        return;
    }

}

void SCHCAckOnErrorSender_WAIT_X_ACK::release()
{
}
