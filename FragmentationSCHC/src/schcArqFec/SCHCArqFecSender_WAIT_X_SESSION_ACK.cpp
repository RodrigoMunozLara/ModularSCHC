#include "schcArqFec/SCHCArqFecSender_WAIT_X_SESSION_ACK.hpp"
#include "SCHCSession.hpp"
#include "LogHelper.hpp"

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
                /* [SAT-SIM] Almaceno el tiempo del ACK  */
                save_time_ack();

                SPDLOG_DEBUG("Stoping the Rtx All-1 timer...");
                _ctx._timer.stop();
                _ctx._rtxAttemptsCounter = 0;
                
                /* ******** Print results in logfile ************************* */
                SPDLOG_DEBUG("Printing results in logfile...");
                std::ostringstream ss;
                std::vector<long long> numeros = _ctx._schcSession._msgTimes_vector;
                for (size_t i = 0; i < numeros.size(); ++i) 
                {
                    ss << numeros[i];
                    if (i < numeros.size() - 1) 
                    {
                        ss << ", ";
                    }
                }

                std::ostringstream ssTypes;
                std::vector<int> types = _ctx._schcSession._msgTimesType_vector;
                for (size_t i = 0; i < types.size(); ++i) 
                {
                    ssTypes << types[i];
                    if (i < types.size() - 1) 
                    {
                        ssTypes << ", ";
                    }
                }

                int _dr;
                if(_ctx._appConfig.lorawan_node.data_rate.compare("DR0") == 0) _dr = 0;
                else if (_ctx._appConfig.lorawan_node.data_rate.compare("DR1") == 0) _dr = 1;
                else if (_ctx._appConfig.lorawan_node.data_rate.compare("DR2") == 0) _dr = 2;
                else if (_ctx._appConfig.lorawan_node.data_rate.compare("DR3") == 0) _dr = 3;
                else if (_ctx._appConfig.lorawan_node.data_rate.compare("DR4") == 0) _dr = 4;
                else if (_ctx._appConfig.lorawan_node.data_rate.compare("DR5") == 0) _dr = 5;
                std::string resultado = std::to_string(_ctx._schcCore._packetCounter) + 
                                            ", " + 
                                            std::to_string(_ctx._appConfig.schc.error_prob) + 
                                            ", " + 
                                            std::to_string(_dr) + 
                                            ", " +
                                            ss.str();

                std::string resultado2 = std::to_string(_ctx._schcCore._packetCounter) + 
                            ", " + 
                            std::to_string(_ctx._appConfig.schc.error_prob) + 
                            ", " + 
                            std::to_string(_dr) + 
                            ", " +
                            ssTypes.str();
                auto file_log = get_file_logger();
                if (file_log) 
                {
                    file_log->info(resultado);
                    file_log->info(resultado2);
                }

                /* ******** Print results in logfile ************************* */

                SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_SESSION_ACK --> STATE_TX_END");
                _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_END;
                _ctx.executeAgain();
                return;
                
            }
            else
            {
                SPDLOG_ERROR("Receiving a SCHC ACK message with parameters (C=1 and W=3). Ignoring message");    
                return; 
            }
        }
        else if (msg_type == SCHCMsgType::SCHC_COMPOUND_ACK)
        {
            SPDLOG_DEBUG("Receiving a SCHC Compound ACK");

            /* [SAT-SIM] Almacenamos el tiempo del ACK Compound para post-procesamiento */
            save_time_ack();

            SPDLOG_DEBUG("Stoping the Retransmission timer...");
            _ctx._timer.stop();
            _ctx._rtxAttemptsCounter = 0;

            decoder.decodeMsg(_ctx._protoType, _ctx._ruleID, msg, SCHCAckMechanism::ACK_COMPOUND, &(_ctx._bitmapArray));
            uint8_t c               = decoder.get_c();
            _ctx._win_with_errors   = decoder.get_w_vector();

            decoder.print_msg(SCHCMsgType::SCHC_COMPOUND_ACK, msg, _ctx._bitmapArray);
            SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_SESSION_ACK --> STATE_TX_RESEND_MISSING_FRAG");
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
    SCHCNodeMessage encoder;
        
    /* Crea un mensaje SCHC en formato hexadecimal */
    std::vector<uint8_t>   schc_message = encoder.create_ack_request(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow);

    /* Imprime los mensajes para visualizacion ordenada */
    encoder.print_msg(SCHCMsgType::SCHC_ACK_REQ_MSG, schc_message); 

    /* [SAT-SIM] Almaceno el tiempo del ACK REQ para post-procesamiento */
    save_time_ack_req();

    /* Envía el mensaje a la capa 2*/
    _ctx._stack->send_frame(_ctx._ruleID, schc_message);

    SPDLOG_DEBUG("There are no more windows with missing tiles.");
    SPDLOG_DEBUG("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_SESSION_ACK");
    _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_WAIT_x_SESSION_ACK;

    // if(_ctx._rtxAttemptsCounter < _ctx._maxAckReq)
    // {
    //     SPDLOG_DEBUG("_rtxAttemptsCounter: {}", _ctx._rtxAttemptsCounter);
    //     SPDLOG_DEBUG("_maxAckReq:          {}", _ctx._maxAckReq);
    //     SPDLOG_DEBUG("Setting retransmission timer to {} seconds", _ctx._retransTimer);
    //     _ctx.executeTimer(_ctx._retransTimer);
    //     _ctx._rtxAttemptsCounter++; 
    // }
    // else
    // {
    //     SPDLOG_DEBUG("_rtxAttemptsCounter: {}", _ctx._rtxAttemptsCounter);
    //     SPDLOG_DEBUG("_maxAckReq:          {}", _ctx._maxAckReq);
    //     SPDLOG_DEBUG("Maximum number of retransmissions reached");
    //     SPDLOG_DEBUG("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_END");
    //     _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_END;
    //     _ctx.executeAgain();
    //     return;       
    // }

}

void SCHCArqFecSender_WAIT_X_SESSION_ACK::release()
{
}

void SCHCArqFecSender_WAIT_X_SESSION_ACK::save_time_ack()
{
    int sat_ptr                         = _ctx._schcSession._sat_win_ptr;       // puntero a la ventana de visibilidad actual en el vector de visibilidad
    _ctx._schcSession._acumulative_win  = _ctx._schcSession._acumulative_win + 
                            _ctx._schcSession._visibility_col[sat_ptr]*1000 + 
                            _ctx._schcSession._revisit_col[sat_ptr]*1000;

    _ctx._schcSession._startTime = std::chrono::steady_clock::now();
    auto elapsed_sim = std::chrono::milliseconds(_ctx._schcSession._acumulative_win);
    _ctx._schcSession._msgTimes_vector.push_back(elapsed_sim.count());
    _ctx._schcSession._msgTimesType_vector.push_back(2);
    SPDLOG_DEBUG("[SAT-SIM] Sending msg in visibility win {}", sat_ptr + 2);
    SPDLOG_DEBUG("[SAT-SIM] Elapsed: {} ms", elapsed_sim.count());
    SPDLOG_DEBUG("[SAT-SIM] Acumulative Win: {} ms", _ctx._schcSession._acumulative_win);                
    _ctx._schcSession._sat_win_ptr++;
}

void SCHCArqFecSender_WAIT_X_SESSION_ACK::save_time_ack_req()
{
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _ctx._schcSession._startTime);
    int sat_ptr                     = _ctx._schcSession._sat_win_ptr;                   // puntero a la ventana de visibilidad actual en el vector de visibilidad

    auto elapsed_sim = elapsed + std::chrono::milliseconds(_ctx._schcSession._acumulative_win);

    _ctx._schcSession._msgTimes_vector.push_back(elapsed_sim.count());
    _ctx._schcSession._msgTimesType_vector.push_back(4);
    SPDLOG_DEBUG("[SAT-SIM] Sending msg in visibility win {}", sat_ptr + 1);
    SPDLOG_DEBUG("[SAT-SIM] Elapsed: {} ms", elapsed_sim.count());
    SPDLOG_DEBUG("[SAT-SIM] Acumulative Win: {} ms", _ctx._schcSession._acumulative_win);
}
