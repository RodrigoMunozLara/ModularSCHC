#include "SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG.hpp"
#include "SCHCAckOnErrorReceiver.hpp"

SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG(SCHCAckOnErrorReceiver &ctx): _ctx(ctx)
{
}

SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::~SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG()
{
}

void SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::execute(const std::vector<uint8_t>& msg)
{
    SCHCGWMessage   decoder;
    SCHCMsgType     msg_type;       // message type decoded. See message type in SCHC_GW_Macros.hpp
    uint8_t         w;              // w recibido en el mensaje
    uint8_t         dtag;           // dtag recibido en el mensajes
    uint8_t         fcn;            // fcn recibido en el mensaje
    char*           payload;
    int             payload_len;    // in bits
    uint32_t        rcs;

    msg_type = decoder.get_msg_type(ProtocolType::LORAWAN, _ctx._ruleID, msg);

    if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_WIN)
    {
        if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
        {
            // if(_counter == 5)
            // {
            //         SPDLOG_DEBUG("\033[31mMessage discarded due to error probability\033[0m");   
            //         _counter++;
            //         return 0;
            // }
            // _counter++;

            SPDLOG_DEBUG("Receiving a SCHC Regular fragment");

            /* Decoding el SCHC fragment */
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
            payload_len     = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
            fcn             = decoder.get_fcn();
            w               = decoder.get_w();


            /* Creacion de buffer para almacenar el schc payload del SCHC fragment */
            std::vector<uint8_t> payload = decoder.get_schc_payload();                  // obtiene el SCHC payload

            /* Obteniendo la cantidad de tiles en el mensaje */
            int tiles_in_payload = (payload_len/8)/_ctx._tileSize;

            /* Se almacenan los tiles en el mapa de recepción de tiles */
            int tile_ptr    = get_tile_ptr(w, fcn);   // tile_ptr: posicion donde se debe almacenar el tile en el _tileArray.
            int bitmap_ptr  = get_bitmap_ptr(fcn);    // bitmap_ptr: posicion donde se debe comenzar escribiendo un 1 en el _bitmapArray.
            for(int i=0; i<tiles_in_payload; i++)
            {
                std::copy(payload.begin() + (i * _ctx._tileSize), payload.begin() + ((i + 1) * _ctx._tileSize),  _ctx._tilesArray[tile_ptr + i].begin());
                _ctx._bitmapArray[w][bitmap_ptr + i] = 1;                                    // en el bitmap, se establece en 1 los correspondientes tiles recibidos
            }

            /* Se almacena el puntero al siguiente tile esperado */
            if((tile_ptr + tiles_in_payload) > _ctx._currentTile_ptr)
            {
                _ctx._currentTile_ptr = tile_ptr + tiles_in_payload;
                SPDLOG_DEBUG("Updating _currentTile_ptr. New value is: {}", _ctx._currentTile_ptr);
            }
            else
            {
                SPDLOG_DEBUG("_currentTile_ptr is not updated. The previous value is kept {}", _ctx._currentTile_ptr);
            }

            /* Se imprime mensaje de la llegada de un SCHC fragment*/
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|--- W={:<1}, FCN={:<2} --->| {:>2} tiles", w, fcn, tiles_in_payload);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

            /* Valida si se han recibido todos los tiles retransmitidos por el sender */
            uint8_t c       = get_c_from_bitmap(w);
            bool valid_rcs  = check_rcs(_ctx._rcs);
            
            if((c == 1) || valid_rcs)
            {
                /* c=1 cuando se reciben todos los tiles de una ventana. 
                valid_rcs=true cuando se reciben todos los tiles */
                if(valid_rcs)
                {
                    /* si valid_rcs == true, entonces era la ultima ventana */
                    SPDLOG_DEBUG("Sending SCHC ACK");
                    SCHCGWMessage    encoder;
                    std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(w); // obtiene el bitmap expresado como un arreglo de char    
                    std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, 1, bitmap_vector);

                    _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                    SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, 1, get_bitmap_array_str(w));
                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                    SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_x_MISSING_FRAGS --> STATE_RX_END");
                    _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
                    _ctx.executeAgain();
                    return;

                    //_ctx._wait_pull_ack_req_flag = true;
                }
                else if(!valid_rcs)
                {
                    /* si c==1 y valid_rcs==false No era la ultima ventana */
                    SPDLOG_DEBUG("Sending SCHC ACK");

                    SCHCGWMessage encoder;
                    int             len;
                    std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(w); // obtiene el bitmap expresado como un arreglo de char    
                    std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);

                    _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                    SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, c, get_bitmap_array_str(w));
                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                    SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_x_MISSING_FRAGS --> STATE_RX_RCV_WINDOW");
                    _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_RCV_WINDOW;

                    _ctx._wait_pull_ack_req_flag = true;               
                }
            }
        }
        else if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
        {
            /* Una forma de saber que la ventana de retransmisión 
             ya ha finalizado es recibiendo un All-1. En ese caso 
             se debe enviar un ACK */

            SPDLOG_DEBUG("Receiving a SCHC All-1 message");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);

            _ctx._lastTileSize  = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
            w                   = decoder.get_w();
            _ctx._rcs           = decoder.get_rcs();
            fcn                 = decoder.get_fcn();
            _ctx._lastTile      = decoder.get_schc_payload();

            bool rcs_result = this->check_rcs(_ctx._rcs);

            if(rcs_result)  // * Integrity check: success
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: success", w, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage         encoder;
                uint8_t c                           = 1;
                std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(w); 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_END");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;

                _ctx._wait_pull_ack_req_flag = true;
            }
            else            // * Integrity check: failure
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: failure", w, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");

                SCHCGWMessage         encoder;
                uint8_t c                           = 0;
                std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(w); 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._wait_pull_ack_req_flag = true;
                _ctx._first_ack_sent_flag    = true;
            }
        }
        else if(msg_type == SCHCMsgType::SCHC_ACK_REQ_MSG)
        {
            if(_ctx._wait_pull_ack_req_flag == true)
            {
                SPDLOG_DEBUG("Receiving SCHC ACK REQ");
                decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);

                w = decoder.get_w();

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|--- ACK REQ, W={:<1} -->| pull ACK REQ discarded", w);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._wait_pull_ack_req_flag = false;
            }
            else
            {
                /* Una forma de saber que la ventana de retransmisión 
                ya ha finalizado es recibiendo un ACK REQ. En ese caso 
                se debe enviar un ACK. Si el SCHC GW recibe un ACK Req 
                (que no es para un push ACK) puede ser por dos motivos.
                
                Motivo 1: Se envió el ACK pero se perdió. Si sigo en 
                    este estado, es porque se envió un ACK con un c = 0. Si 
                    hubiese sido un c = 1, se hubiese cambiado de estado a 
                    RX_RCV_WIN_recv_fragments()

                Motivo 2: Nunca se envió un ACK porque no se detectó 
                    el fin de la ventana debido a la pérdida de algun mensaje.
                    Por lo tanto c = 0 */

                SPDLOG_DEBUG("Receiving SCHC ACK REQ");

                decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
                w = decoder.get_w();

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|--- ACK REQ, W={:<1} -->|", w);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage         encoder;
                uint8_t c               = 0;
                std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(w); 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._wait_pull_ack_req_flag = true;

            }

        }
        else
        {
            SPDLOG_ERROR("Receiving an unexpected type of message. Discarding message");
        }
    }
    else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_SES)
    {
        if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
        {
            SPDLOG_DEBUG("Receiving a SCHC Regular fragment");

            /* Decoding el SCHC fragment */
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
            payload_len     = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
            fcn             = decoder.get_fcn();
            w               = decoder.get_w();

            if(w > _ctx._last_window)
                _ctx._last_window    = w;    // aseguro que el ultimo fragmento recibido va a marcar cual es la ultima ventana recibida


            /* Creacion de buffer para almacenar el schc payload del SCHC fragment */
            std::vector<uint8_t> payload = decoder.get_schc_payload();                // obtiene el SCHC payload

            /* Obteniendo la cantidad de tiles en el mensaje */
            int tiles_in_payload = (payload_len/8)/_ctx._tileSize;

            /* Se almacenan los tiles en el mapa de recepción de tiles */
            int tile_ptr    = this->get_tile_ptr(w, fcn);   // tile_ptr: posicion donde se debe almacenar el tile en el _tileArray.
            int bitmap_ptr  = this->get_bitmap_ptr(fcn);    // bitmap_ptr: posicion donde se debe comenzar escribiendo un 1 en el _bitmapArray.
            for(int i=0; i<tiles_in_payload; i++)
            {
                std::copy(payload.begin() + (i * _ctx._tileSize), payload.begin() + ((i + 1) * _ctx._tileSize),  _ctx._tilesArray[tile_ptr + i].begin());
                _ctx._bitmapArray[w][bitmap_ptr + i] = 1;             
            }


            /* Se almacena el puntero al siguiente tile esperado */
            if((tile_ptr + tiles_in_payload) > _ctx._currentTile_ptr)
            {
                _ctx._currentTile_ptr = tile_ptr + tiles_in_payload;
                SPDLOG_DEBUG("Updating _currentTile_ptr. New value is: {}", _ctx._currentTile_ptr);
            }
            else
            {
                SPDLOG_DEBUG("_currentTile_ptr is not updated. The previous value is kept {}", _ctx._currentTile_ptr);
            }

            /* Se imprime mensaje de la llegada de un SCHC fragment*/
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|--- W={:<1}, FCN={:<2} --->| {:>2} tiles", w, fcn, tiles_in_payload);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

            /* Valida en el bitmap si se han recibido todos los tiles retransmitidos por el sender */
            uint8_t c = this->get_c_from_bitmap(w);

            if(c==1 && w!=_ctx._last_window)
            {
                int next_window = w + 1;
                if(next_window == _ctx._last_window)
                {
                    int len;
                    char* buffer        = nullptr;
                    bool valid_rcs = this->check_rcs(_ctx._rcs);

                    if(valid_rcs)
                    {
                        SPDLOG_DEBUG("Sending SCHC ACK");
                        SCHCGWMessage    encoder;
                        std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(next_window); // obtiene el bitmap expresado como un arreglo de char    
                        std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, next_window, 1, bitmap_vector);

                        _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                        //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                        SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", next_window, 1, get_bitmap_array_str(next_window));
                        //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                        SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_x_MISSING_FRAGS --> STATE_RX_END");
                        _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;

                    }
                    else
                    {
                        SPDLOG_DEBUG("Sending SCHC ACK");

                        SCHCGWMessage    encoder;
                        c                                   = 0;
                        std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(_ctx._last_window); // obtiene el bitmap expresado como un arreglo de char    
                        std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, _ctx._last_window, 1, bitmap_vector);

                        _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                        //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                        SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", _ctx._last_window, c, get_bitmap_array_str(_ctx._last_window));
                        //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
                        _ctx._last_confirmed_window = _ctx._last_window; 

                        _ctx._wait_pull_ack_req_flag = true;
                    }
                }
                else
                {
                    for(int i = next_window; i<_ctx._last_window; i++)
                    {
                        int len;
                        char* buffer    = nullptr;
                        int c_i         = get_c_from_bitmap(i);
                        if(c_i == 0)
                        {
                            SPDLOG_DEBUG("Sending SCHC ACK");

                            SCHCGWMessage    encoder;
                            std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(i); // obtiene el bitmap expresado como un arreglo de char    
                            std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, i, c_i, bitmap_vector);

                            _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                            SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", i, c_i, get_bitmap_array_str(i));
                            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                            _ctx._last_confirmed_window = i; 
                            _ctx._wait_pull_ack_req_flag = true;
                            break;  
                        }
                        else
                        {
                            SPDLOG_DEBUG("The SCHC gateway correctly received the tiles for window {}.", i);
                        }

                    }// cierre del for
                }
            }
            else if(c==1 && w==_ctx._last_window)
            {
                int len;
                char* buffer        = nullptr;
                bool valid_rcs = this->check_rcs(_ctx._rcs);
                if(valid_rcs)
                {
                    SPDLOG_DEBUG("Sending SCHC ACK");
                    SCHCGWMessage    encoder;
                    std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(_ctx._last_window); // obtiene el bitmap expresado como un arreglo de char    
                    std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, _ctx._last_window, 1, bitmap_vector);

                    _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                    SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", _ctx._last_window, 1, get_bitmap_array_str(_ctx._last_window));
                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                    SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_x_MISSING_FRAGS --> STATE_RX_END");
                    _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
                }
                else
                {
                    SPDLOG_DEBUG("Sending SCHC ACK");

                    SCHCGWMessage    encoder;
                    std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(_ctx._last_window); // obtiene el bitmap expresado como un arreglo de char    
                    std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, _ctx._last_window, c, bitmap_vector);

                    _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                    SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", _ctx._last_window, c, get_bitmap_array_str(_ctx._last_window));
                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
                    _ctx._last_confirmed_window  = _ctx._last_window; 

                    _ctx._wait_pull_ack_req_flag = true;
                }
            }
        }
        else if(msg_type == SCHCMsgType::SCHC_ACK_REQ_MSG)
        {
            if(_ctx._wait_pull_ack_req_flag == true)
            {
                SPDLOG_DEBUG("Receiving SCHC ACK REQ");

                decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
                w = decoder.get_w();

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|--- ACK REQ, W={:<1} -->| pull ACK REQ discarded", w);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
                _ctx._wait_pull_ack_req_flag = false;
            }
            else
            {
                SPDLOG_DEBUG("Receiving SCHC ACK REQ");

                decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
                uint8_t w_received = decoder.get_w();

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|--- ACK REQ, W={:<1} -->| ", w_received);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                /* Revisa cual ventana tiene errores y envia un ACK para esa ventana */
                for(int i = _ctx._last_confirmed_window; i<_ctx._last_window; i++)
                {
                    int len;
                    char* buffer                = nullptr;
                    uint8_t c                   = get_c_from_bitmap(i);
                    if(c == 0)
                    {
                        SPDLOG_DEBUG("Sending SCHC ACK");

                        SCHCGWMessage    encoder;
                        _ctx._last_confirmed_window         = i;
                        std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(i); 
                        std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, i, c, bitmap_vector);

                        _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                        //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                        SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", i, c, get_bitmap_array_str(i));
                        //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                        _ctx._wait_pull_ack_req_flag = true;

                        return;                     
                    }
                    else
                    {
                        SPDLOG_DEBUG("SCHC Window {} has received all tiles. No ACK sent", i);
                    }
                }

                /* Si llegó a esta parte del codigo es porque ninguna ventana tiene errores. 
                Por lo tanto la ventana con errores es la última.*/
                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage    encoder;
                int c                               = 0;
                std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(_ctx._last_window); 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, _ctx._last_window, c, bitmap_vector);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", _ctx._last_window, c, get_bitmap_array_str(_ctx._last_window));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
                _ctx._last_confirmed_window = _ctx._last_window; 

                _ctx._wait_pull_ack_req_flag = true;

            }
            
        }
        else if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
        {
            SPDLOG_DEBUG("Receiving a SCHC All-1 message");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);


            _ctx._lastTileSize  = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
            w                   = decoder.get_w();
            _ctx._last_window   = w;
            _ctx._rcs           = decoder.get_rcs();
            fcn                 = decoder.get_fcn();
            _ctx._lastTile      = decoder.get_schc_payload();


            bool rcs_result = this->check_rcs(_ctx._rcs);

            if(rcs_result)  // * Integrity check: success
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: success", w, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage         encoder;
                uint8_t c                           = get_c_from_bitmap(w);
                std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(w); 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);
                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_END");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;


            }
            else            // * Integrity check: failure
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: failure", w, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");

                SCHCGWMessage         encoder;
                uint8_t c                           = 0;
                std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(w); 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;

                _ctx._wait_pull_ack_req_flag = true;
            }
        }

    }
    else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_COMPOUND)
    {
        if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
        {
            SPDLOG_DEBUG("Receiving a SCHC Regular fragment");

            // if(_counter == 6)
            // {
            //         SPDLOG_WARN("\033[31mMessage discarded due to error probability\033[0m");   
            //         _counter++;
            //         return 0;
            // }
            // _counter++;


            /* Decoding el SCHC fragment */
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
            payload_len     = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
            fcn             = decoder.get_fcn();
            w               = decoder.get_w();            

            if(w > _ctx._last_window)
                _ctx._last_window    = w;    // aseguro que el ultimo fragmento recibido va a marcar cual es la ultima ventana recibida


            /* Creacion de buffer para almacenar el schc payload del SCHC fragment */
            std::vector<uint8_t> payload = decoder.get_schc_payload();                  // obtiene el SCHC payload


            /* Obteniendo la cantidad de tiles en el mensaje */
            int tiles_in_payload = (payload_len/8)/_ctx._tileSize;


            /* Se almacenan los tiles en el mapa de recepción de tiles */
            int tile_ptr    = get_tile_ptr(w, fcn);   // tile_ptr: posicion donde se debe almacenar el tile en el _tileArray.
            int bitmap_ptr  = get_bitmap_ptr(fcn);    // bitmap_ptr: posicion donde se debe comenzar escribiendo un 1 en el _bitmapArray.
            for(int i=0; i<tiles_in_payload; i++)
            {         
                std::copy(payload.begin() + (i * _ctx._tileSize), payload.begin() + ((i + 1) * _ctx._tileSize),  _ctx._tilesArray[tile_ptr + i].begin());
                _ctx._bitmapArray[w][bitmap_ptr + i] = 1;                                    // en el bitmap, se establece en 1 los correspondientes tiles recibidos
            }


            /* Se almacena el puntero al siguiente tile esperado */
            if((tile_ptr + tiles_in_payload) > _ctx._currentTile_ptr)
            {
                _ctx._currentTile_ptr = tile_ptr + tiles_in_payload;
                SPDLOG_DEBUG("Updating _currentTile_ptr. New value is: {}", _ctx._currentTile_ptr);
            }
            else
            {
                SPDLOG_DEBUG("_currentTile_ptr is not updated. The previous value is kept {}", _ctx._currentTile_ptr);
            }

            /* Se imprime mensaje de la llegada de un SCHC fragment*/
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|--- W={:<1}, FCN={:<2} --->| {:>2} tiles", w, fcn, tiles_in_payload);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


            bool rcs_result = this->check_rcs(_ctx._rcs);
            if(rcs_result)
            {
                SPDLOG_DEBUG("Sending SCHC Compound ACK");
                SCHCGWMessage    encoder;
                std::vector<uint8_t>    windows_with_error;  
                std::vector<uint8_t> buffer         = encoder.create_schc_ack_compound(_ctx._ruleID, _ctx._dTag, _ctx._last_window, windows_with_error, _ctx._bitmapArray, _ctx._windowSize);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, C=1 -------| {}", encoder.get_compound_bitmap_str());
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_x_MISSING_FRAGS --> STATE_RX_END");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;

                _ctx._wait_pull_ack_req_flag = true;
            }
  
        }        
        else if(msg_type == SCHCMsgType::SCHC_ACK_REQ_MSG)
        {
            SPDLOG_DEBUG("Receiving SCHC ACK REQ");
            if(_ctx._wait_pull_ack_req_flag == true)
            {
                SPDLOG_DEBUG("Receiving SCHC ACK REQ");

                decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
                w = decoder.get_w();

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|--- ACK REQ, W={:<1} -->| pull ACK REQ discarded", w);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
                _ctx._wait_pull_ack_req_flag = false;
            }
            else
            {
                SPDLOG_DEBUG("Receiving SCHC ACK REQ");

                decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
                w = decoder.get_w();

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|--- ACK REQ, W={:<1} -->| ", w);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                bool rcs_result = this->check_rcs(_ctx._rcs);
                if(rcs_result)
                {
                    SPDLOG_DEBUG("Sending SCHC Compound ACK");
                    SCHCGWMessage    encoder;
                    std::vector<uint8_t>    windows_with_error;  
                    std::vector<uint8_t> buffer         = encoder.create_schc_ack_compound(_ctx._ruleID, _ctx._dTag, _ctx._last_window, windows_with_error, _ctx._bitmapArray, _ctx._windowSize);

                    _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                    SPDLOG_INFO("|<-- ACK, C=1 -------| {}", encoder.get_compound_bitmap_str());
                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                    SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_x_MISSING_FRAGS --> STATE_RX_END");
                    _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;

                    _ctx._wait_pull_ack_req_flag = true;
                }
                else
                {
                    SPDLOG_DEBUG("Sending SCHC Compound ACK");
                    /* Revisa si alguna ventana tiene tiles perdidos. Si encuentra alguna, la almacena en un vector */
                    std::vector<uint8_t> windows_with_error;
                    for(int i=0; i < _ctx._last_window; i++)
                    {
                        int c = this->get_c_from_bitmap(i);
                        if(c == 0)
                            windows_with_error.push_back(i);
                    }
                    windows_with_error.push_back(_ctx._last_window);

                    SCHCGWMessage    encoder;
                    std::vector<uint8_t> buffer         = encoder.create_schc_ack_compound(_ctx._ruleID, _ctx._dTag, _ctx._last_window, windows_with_error, _ctx._bitmapArray, _ctx._windowSize);

                    _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);



                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                    SPDLOG_INFO("|<-- ACK, C=0 -------| {}", encoder.get_compound_bitmap_str());
                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                    _ctx._wait_pull_ack_req_flag = true;

                }

            }
              
        }
        else if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
        {
            SPDLOG_DEBUG("Receiving a SCHC All-1 message");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);


            _ctx._lastTileSize  = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
            w                   = decoder.get_w();
            _ctx._last_window   = w;
            _ctx._rcs           = decoder.get_rcs();
            fcn                 = decoder.get_fcn();
            _ctx._lastTile      = decoder.get_schc_payload();

            bool rcs_result = this->check_rcs(_ctx._rcs);

            if(rcs_result)  // * Integrity check: success
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: success", w, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage         encoder;
                uint8_t c                           = get_c_from_bitmap(w);                     // obtiene el valor de c en base al _bitmap_array
                std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(w); 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_x_MISSING_FRAGS --> STATE_RX_END");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
            }
            else            // * Integrity check: failure
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: failure", w, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");

                /* Revisa si alguna ventana tiene tiles perdidos */
                std::vector<uint8_t> windows_with_error;
                for(int i=0; i < _ctx._last_window; i++)
                {
                    int c = this->get_c_from_bitmap(i);
                    if(c == 0)
                        windows_with_error.push_back(i);
                }
                windows_with_error.push_back(_ctx._last_window);
         

                SCHCGWMessage    encoder;
                std::vector<uint8_t> buffer         = encoder.create_schc_ack_compound(_ctx._ruleID, _ctx._dTag, _ctx._last_window, windows_with_error, _ctx._bitmapArray, _ctx._windowSize);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, C=0 -------| {}", encoder.get_compound_bitmap_str());
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._wait_pull_ack_req_flag = true;
            }
        }
    }

}

void SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::timerExpired()
{
}

void SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::release()
{
}

int SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::get_tile_ptr(uint8_t window, uint8_t fcn)
{
    if(window==0)
    {
        return (_ctx._windowSize - 1) - fcn;
    }
    else if(window == 1)
    {
        return (2*_ctx._windowSize - 1) - fcn;
    }
    else if(window == 2)
    {
        return (3*_ctx._windowSize - 1) - fcn;
    }
    else if(window == 3)
    {
        return (4*_ctx._windowSize - 1) - fcn;
    }
    return -1;
}

int SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::get_bitmap_ptr(uint8_t fcn)
{
    return (_ctx._windowSize - 1) - fcn;
}

uint8_t SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::get_c_from_bitmap(uint8_t window)
{
    /* La funcion indica si faltan tiles para la ventana pasada como argumento.
    Retorna un 1 si no faltan tiles y 0 si faltan tiles */

    for (int i=0; i<_ctx._windowSize; i++)
    {
        if(_ctx._bitmapArray[window][i] == 0)
            return 0;
    }

    return 1;
}

bool SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::check_rcs(uint32_t rcs)
{
    // Calcular el tamaño total necesario para el buffer de todos los tiles
    int total_size = (_ctx._currentTile_ptr * _ctx._tileSize) + _ctx._lastTileSize/8;

    // Crear un vector para almacenar todos los valores
    std::vector<uint8_t> buffer;
    buffer.reserve(total_size);

    int k=0;
    for(int i=0; i<_ctx._currentTile_ptr; i++)
    {
        buffer.insert(buffer.end(), _ctx._tilesArray[i].begin(), _ctx._tilesArray[i].end());
    }


    size_t bytesToCopy = _ctx._lastTileSize;

    if (bytesToCopy > 0) 
    {
        buffer.insert(buffer.end(), _ctx._lastTile.begin(), _ctx._lastTile.end());
    }

    //SPDLOG_DEBUG("buffer: {:Xp}", spdlog::to_hex(buffer));

    uint32_t rcs_calculed = this->calculate_crc32(buffer);

    SPDLOG_DEBUG("calculated RCS: {}", rcs_calculed);
    SPDLOG_DEBUG("  received RCS: {}", rcs);

    if(rcs_calculed == rcs)
        return true;
    else
        return false;
}

uint32_t SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::calculate_crc32(const std::vector<uint8_t>& data) 
{
    //SPDLOG_DEBUG("Message in hex: {:Xp}", spdlog::to_hex(data));
    //SPDLOG_DEBUG("Message size: {}", data.size());
    // Polinomio CRC32 (reflejado) - Estándar IEEE 802.3
    const uint32_t polynomial = 0xEDB88320;
    uint32_t crc = 0xFFFFFFFF;

    // Usamos un loop basado en rangos para mayor claridad y seguridad
    for (uint8_t byte : data) {
        crc ^= byte;

        // Procesar los 8 bits del byte
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}

std::vector<uint8_t> SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::get_bitmap_array_vec(uint8_t window)
{
    std::vector<uint8_t> bitmap_v;
    for(int i=0; i<_ctx._windowSize; i++)
    {
        bitmap_v.push_back(_ctx._bitmapArray[window][i]);
    }

    return bitmap_v;
}

std::string SCHCAckOnErrorReceiver_WAIT_X_MISSING_FRAG::get_bitmap_array_str(uint8_t window)
{
    std::string bitmap_str = "";
    for(int i=0; i<_ctx._windowSize; i++)
    {
        bitmap_str = bitmap_str + std::to_string(_ctx._bitmapArray[window][i]);
    }
    return bitmap_str;
}