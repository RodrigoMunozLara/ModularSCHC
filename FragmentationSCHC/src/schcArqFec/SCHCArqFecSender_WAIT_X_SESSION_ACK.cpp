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

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _ctx._schcSession._startTime).count();
            _ctx._schcSession._msgTimes_vector.push_back(elapsed);
            _ctx._schcSession._msgTimesType_vector.push_back(2);

            decoder.decodeMsg(_ctx._protoType, _ctx._ruleID, msg, SCHCAckMechanism::ARQ_FEC, &(_ctx._bitmapArray));
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);

            if(c == 1 && w == 3)
            {
                SPDLOG_DEBUG("Stoping the Rtx All-1 timer...");
                _ctx._timer.stop();
                
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
    SCHCNodeMessage encoder;        // encoder 

    /* Crea un mensaje SCHC en formato hexadecimal */
    std::vector<uint8_t> schc_all_1_message = encoder.create_all_1_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._rcs, _ctx._lastTile);

    /* Imprime los mensajes para visualizacion ordenada */
    encoder.print_msg(SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message); 

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _ctx._schcSession._startTime).count();
    _ctx._schcSession._msgTimes_vector.push_back(elapsed);
    _ctx._schcSession._msgTimesType_vector.push_back(3);

    /* Envía el mensaje a la capa 2*/
    _ctx._stack->send_frame(_ctx._ruleID, schc_all_1_message);

    SPDLOG_DEBUG("Setting retransmission timer to {} seconds", _ctx._retransTimer);
    _ctx.executeTimer(_ctx._retransTimer);
}

void SCHCArqFecSender_WAIT_X_SESSION_ACK::release()
{
}

