#include "schcAckOnError/SCHCAckOnErrorReceiver_RCV_WINDOW.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver.hpp"


SCHCAckOnErrorReceiver_RCV_WINDOW::SCHCAckOnErrorReceiver_RCV_WINDOW(SCHCAckOnErrorReceiver &ctx): _ctx(ctx)
{
    if(_ctx._appConfig.schc.schc_l2_protocol.compare("lorawan_ns_mqtt") == 0)
    {
        /* Dynamic SCHC parameters */
        _ctx._currentWindow = 0;
        _ctx._currentFcn = (_ctx._windowSize)-1;
        _ctx._currentBitmap_ptr = 0;
        _ctx._currentTile_ptr = 0;

        /* memory allocated for vector of each tile. */
        _ctx._tilesArray.resize(_ctx._nTotalTiles);

        /* memory allocated for each tile. */ 
        for(int i = 0 ; i < _ctx._nTotalTiles ; i++ )
        {
            _ctx._tilesArray[i].resize(_ctx._tileSize);
        }      

        _ctx._lastTile.resize(_ctx._tileSize);

        /* memory allocated for pointers of each bitmap. */
        _ctx._bitmapArray.resize(_ctx._nWindows);

        /* memory allocated for the 1s and 0s for each bitmap. */ 
        for(int i = 0 ; i < _ctx._nWindows ; i++ )
        {
            _ctx._bitmapArray[i].resize(_ctx._windowSize);
        }



    }
}

SCHCAckOnErrorReceiver_RCV_WINDOW::~SCHCAckOnErrorReceiver_RCV_WINDOW()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorReceiver_RCV_WINDOW destructor()");    
}

void SCHCAckOnErrorReceiver_RCV_WINDOW::execute(const std::vector<uint8_t>& msg)
{

    SCHCGWMessage           decoder;
    SCHCMsgType             msg_type;       // message type decoded. See message type in SCHC_GW_Macros.hpp
    uint8_t                 w;              // w recibido en el mensaje
    uint8_t                 dtag = 0;       // dtag no es usado en LoRaWAN
    uint8_t                 fcn;            // fcn recibido en el mensaje
    std::vector<uint8_t>    payload;
    int                     payload_len;    // in bits

    SPDLOG_DEBUG("Decoding Message...");
    msg_type = decoder.get_msg_type(ProtocolType::LORAWAN, _ctx._ruleID, msg);

    if(msg_type == SCHCMsgType::SCHC_ACK_REQ_MSG && !_ctx._enable_to_process_msg)
    {
        SPDLOG_DEBUG("Receiving a message that does not belong to the session. Discarting Message...");
        SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_END");
        _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
        _ctx.executeAgain();
        return;

    }


    if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_WIN)
    {
        if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
        {
            _ctx._enable_to_process_msg = true;

            /* Codigo para poder eliminar mensajes de entrada */
            // random_device rd;
            // mt19937 gen(rd());
            // uniform_int_distribution<int> dist(0, 100);
            // int random_number = dist(gen);
            // //if(random_number < _error_prob)
            if(_ctx._counter == 2 || _ctx._counter == 4 ||  _ctx._counter == 12)
            {
                    //SPDLOG_WARN("\033[31mMessage discarded due to error probability\033[0m");   
                    SPDLOG_INFO("\033[31mMessage discarded due to error probability\033[0m");   
                    _ctx._counter++;
                    return;
            }
            _ctx._counter++;
            

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
                // se almacenan los bytes de un tile recibido
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
            SPDLOG_INFO("|--- W={:<1}, FCN={:<2} ----->| {:>2} tiles", w, fcn, tiles_in_payload);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


            /* Una forma de saber que la ventana de transmisión 
             ya ha finalizado es recibiendo el tile 0 de la ventana. 
             En ese caso se debe enviar un ACK */
            if((fcn - tiles_in_payload) <= 0)
            {
                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage    encoder;
                uint8_t c                           = get_c_from_bitmap(w);    // Bien usado
                std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(w); // obtiene el bitmap expresado como un arreglo de char    
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);
                _ctx._last_ack_sent_buffer = buffer;
                _ctx._last_ack_sent_c = c;
                _ctx._last_ack_sent_w = w;

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} ----| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                if(c==0)
                {
                    SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
                    _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;
                    return;
                }
            }
            
        }
        else if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
        {
            // SPDLOG_DEBUG("\033[31mMessage discarded due to error probability\033[0m");   
            // return 0;

            /* Una forma de saber que la ventana de transmisión 
             ya ha finalizado es recibiendo un All-1. En ese caso 
             se debe enviar un ACK */

            SPDLOG_DEBUG("Receiving a SCHC All-1 message");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);

            _ctx._lastTileSize  = decoder.get_schc_payload_len()/8;   // largo del payload SCHC. En bits
            w                   = decoder.get_w();
            _ctx._rcs           = decoder.get_rcs();
            fcn                 = decoder.get_fcn();
            _ctx._lastTile      = decoder.get_schc_payload();           // obtiene el SCHC payload


            bool rcs_result = check_rcs(_ctx._rcs);

            if(rcs_result)  // * Integrity check: success
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|--- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: success", w, fcn, _ctx._lastTileSize*8);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage encoder;
                uint8_t c                           = 1;                     // obtiene el valor de c en base al _bitmap_array
                std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(w); // obtiene el bitmap expresado como un arreglo de char  
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);  

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);
                _ctx._last_ack_sent_buffer = buffer;
                _ctx._last_ack_sent_c = c;
                _ctx._last_ack_sent_w = w;

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} ----| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_END");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
                _ctx.executeAgain();
                

            }
            else                // * Integrity check: failure
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: failure", w, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage         encoder;
 
                uint8_t c                           = 0;
                std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(w); // obtiene el bitmap expresado como un arreglo de char  
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c, bitmap_vector);  

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);
                _ctx._last_ack_sent_buffer = buffer;
                _ctx._last_ack_sent_c = c;
                _ctx._last_ack_sent_w = w;

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} ----| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;

            }
        
        }
        else if(msg_type == SCHCMsgType::SCHC_ACK_REQ_MSG)
        {    
            /* Una forma de saber que la ventana de transmisión 
            ya ha finalizado es recibiendo un ACK REQ. En ese caso 
            se debe enviar un ACK. Si el SCHC GW recibe un ACK Req 
            (que no es para un push ACK) puede ser por dos motivos. 
            Motivo 1: Se envió el ACK pero se perdió.
            Motivo 2: Nunca se envió un ACK porque no detectó el fin de la ventana. */

            SPDLOG_DEBUG("Receiving SCHC ACK REQ");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
            w = decoder.get_w();

            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|--- ACK REQ, W={:<1} -->| ", w);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

            SPDLOG_DEBUG("Sending SCHC ACK");
            SCHCGWMessage    encoder;
            uint8_t c                           = _ctx._last_ack_sent_c;
            std::vector<uint8_t> buffer         = _ctx._last_ack_sent_buffer;  

            _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} ----| Bitmap:{}", _ctx._last_ack_sent_w, c, get_bitmap_array_str(_ctx._last_ack_sent_w));
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
            
        }
        else
        {
            SPDLOG_WARN("Receiving an unexpected type of message. Discarding message");
        }
    }
    else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_SES)
    {
        if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
        {
            _ctx._enable_to_process_msg = true;

            SPDLOG_DEBUG("Receiving a SCHC Regular fragment");

            /* Codigo para poder eliminar mensajes de entrada */
            // random_device rd;
            // mt19937 gen(rd());
            // uniform_int_distribution<int> dist(0, 100);
            // int random_number = dist(gen);
            
            //if(random_number < _error_prob)
            if(_ctx._counter == 2 || _ctx._counter == 4 ||  _ctx._counter == 9)
            {
                    //SPDLOG_WARN("\033[31mMessage discarded due to error probability\033[0m");   
                    SPDLOG_INFO("\033[31mMessage discarded due to error probability\033[0m");   
                    _ctx._counter++;
                    return;
            }
            _ctx._counter++;

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
                
                
                if((bitmap_ptr + i) > (_ctx._windowSize - 1))
                {
                    /* ha finalizado la ventana w y ha comenzado la ventana w+1*/
                    _ctx._bitmapArray[w+1][bitmap_ptr + i - _ctx._windowSize] = 1;
                }
                else
                {
                    _ctx._bitmapArray[w][bitmap_ptr + i] = 1;
                }
                
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
            
        }
        else if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG) 
        { 
            // SPDLOG_DEBUG("\033[31mMessage discarded due to error probability\033[0m"); 
            // return 0;

            SPDLOG_DEBUG("Receiving a SCHC All-1 message");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);

            _ctx._lastTileSize  = decoder.get_schc_payload_len()/8;   // largo del payload SCHC. En bits
            _ctx._last_window   = decoder.get_w();
            _ctx._rcs           = decoder.get_rcs();
            fcn                 = decoder.get_fcn();
            _ctx._lastTile      = decoder.get_schc_payload();           // obtiene el SCHC payload

            
            bool rcs_result = this->check_rcs(_ctx._rcs);

            if(rcs_result)    // * Integrity check: success
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: success", _ctx._last_window, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage encoder;
                uint8_t c                           = 1;                     // obtiene el valor de c en base al _bitmap_array
                std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(w); // obtiene el bitmap expresado como un arreglo de char  
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, _ctx._last_window, c, bitmap_vector);  

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", w, c, get_bitmap_array_str(w));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v"); 


                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_END");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
                _ctx.executeAgain();

            }
            else                        // * Integrity check: failure
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: failure", w, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                _ctx._bitmapArray[w][_ctx._windowSize-1] = 1;

                /* Revisa cual ventana tiene errores (a excepcion de la ultima 
                ventana que no la revisa en este for) y envia un ACK para esa ventana */
                for(int i = _ctx._last_confirmed_window; i<_ctx._last_window; i++)
                {
                    uint8_t c                   = get_c_from_bitmap(i); // Bien usado
                    if(c == 0)
                    {
                        SPDLOG_DEBUG("Sending SCHC ACK");
                        SCHCGWMessage encoder;
                        _ctx._last_confirmed_window         = i;
                        std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(i); // obtiene el bitmap expresado como un arreglo de char  
                        std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, i, c, bitmap_vector);  

                        _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

                        //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                        SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", i, c, get_bitmap_array_str(i));
                        //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                        SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
                        _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;

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
                SCHCGWMessage encoder;
                int c                               = 0;
                std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(_ctx._last_window); // obtiene el bitmap expresado como un arreglo de char  
                std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, _ctx._last_window, c, bitmap_vector);  

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}",_ctx._last_window, c, get_bitmap_array_str(_ctx._last_window));
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
                _ctx._last_confirmed_window = _ctx._last_window; 

                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;

           }
        }
        else if(msg_type == SCHCMsgType::SCHC_ACK_REQ_MSG)
        {

            SPDLOG_DEBUG("Receiving SCHC ACK REQ");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
            uint8_t w_received          = decoder.get_w();

            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|--- ACK REQ, W={:<1} -->| ", w_received);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

            if(w_received > _ctx._last_window)
                _ctx._last_window    = w_received;    // aseguro que el ultimo fragmento recibido va a marcar cual es la ultima ventana recibida



            /* Revisa cual ventana tiene errores y envia un ACK para esa ventana */
            for(int i = _ctx._last_confirmed_window; i<_ctx._last_window; i++)
            {
                uint8_t c                   = get_c_from_bitmap(i); // Bien usado
                if(c == 0)
                {
                    SPDLOG_DEBUG("Sending SCHC ACK");

                    SCHCGWMessage    encoder;
                    _ctx._last_confirmed_window         = i;
                    std::vector<uint8_t> bitmap_vector  = this->get_bitmap_array_vec(i); // obtiene el bitmap expresado como un arreglo de char  
                    std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, i, c, bitmap_vector);  

                    _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                    SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", i, c, get_bitmap_array_str(i));
                    //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                    SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
                    _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;

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
            uint8_t c                           = 0;
            std::vector<uint8_t> bitmap_vector  = get_bitmap_array_vec(_ctx._last_window); // obtiene el bitmap expresado como un arreglo de char  
            std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, _ctx._last_window, c, bitmap_vector); 

            _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --| Bitmap:{}", _ctx._last_window, c, get_bitmap_array_str(_ctx._last_window));
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");
            _ctx._last_confirmed_window = _ctx._last_window; 

            SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
            _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;
                

        }
        else
        {
            SPDLOG_ERROR("Receiving an unexpected type of message. Discarding message");
        }        
    }
    else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_COMPOUND)
    {
        if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
        {
            _ctx._enable_to_process_msg = true;

            SPDLOG_DEBUG("Receiving a SCHC Regular fragment");

            /* Codigo para poder eliminar mensajes de entrada */
            // random_device rd;
            // mt19937 gen(rd());
            // uniform_int_distribution<int> dist(0, 100);
            // int random_number = dist(gen);
            //if(random_number < _error_prob)
            if(_ctx._counter == 2 || _ctx._counter == 4 ||  _ctx._counter == 9)
            {
                    //SPDLOG_WARN("\033[31mMessage discarded due to error probability\033[0m");   
                    SPDLOG_INFO("\033[31mMessage discarded due to error probability\033[0m");   
                    _ctx._counter++;
                    return;
            }
            _ctx._counter++;

            /* Decoding el SCHC fragment */
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
            payload_len     = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
            fcn             = decoder.get_fcn();
            w               = decoder.get_w();

            if(w > _ctx._last_window)
                _ctx._last_window    = w;    // aseguro que el ultimo fragmento recibido va a marcar cual es la ultima ventana recibida

            /* Creacion de buffer para almacenar el schc payload del SCHC fragment */
            std::vector<uint8_t> payload = decoder.get_schc_payload();

            /* Obteniendo la cantidad de tiles en el mensaje */
            int tiles_in_payload = (payload_len/8)/_ctx._tileSize;

            /* Se almacenan los tiles en el mapa de recepción de tiles */
            int tile_ptr    = get_tile_ptr(w, fcn);   // tile_ptr: posicion donde se debe almacenar el tile en el _tileArray.
            int bitmap_ptr  = get_bitmap_ptr(fcn);    // bitmap_ptr: posicion donde se debe comenzar escribiendo un 1 en el _bitmapArray.

            for(int i=0; i<tiles_in_payload; i++)
            {
                std::copy(payload.begin() + (i * _ctx._tileSize), payload.begin() + ((i + 1) * _ctx._tileSize),  _ctx._tilesArray[tile_ptr + i].begin());
 
                if((bitmap_ptr + i) > (_ctx._windowSize - 1))
                {
                    /* ha finalizado la ventana w y ha comenzado la ventana w+1*/
                    _ctx._bitmapArray[w+1][bitmap_ptr + i - _ctx._windowSize] = 1;
                }
                else
                {
                    _ctx._bitmapArray[w][bitmap_ptr + i] = 1;
                }
                
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
            
        }        
        else if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG) 
        { 
            //SPDLOG_DEBUG("\033[31mMessage discarded due to error probability\033[0m"); 
            //return;

            SPDLOG_DEBUG("Receiving a SCHC All-1 message");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);

            _ctx._lastTileSize  = decoder.get_schc_payload_len()/8;   // largo del payload SCHC. En bits
            _ctx._last_window   = decoder.get_w();
            _ctx._rcs           = decoder.get_rcs();
            fcn                 = decoder.get_fcn();
            _ctx._lastTile      = decoder.get_schc_payload();           // obtiene el SCHC payload

            _ctx._bitmapArray[w][_ctx._windowSize-1]  = 1;
           
            bool rcs_result = this->check_rcs(_ctx._rcs);

            if(rcs_result)    // * Integrity check: success
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: success", _ctx._last_window, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");    

                SPDLOG_DEBUG("Sending SCHC Compound ACK");            
                SCHCGWMessage encoder;
                std::vector<uint8_t> windows_with_error;   // vector vacío  
                std::vector<uint8_t> buffer         = encoder.create_schc_ack_compound(_ctx._ruleID, _ctx._dTag, _ctx._last_window, windows_with_error, _ctx._bitmapArray, _ctx._windowSize); 

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, C=1 -------| {}", encoder.get_compound_bitmap_str());
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_END");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
                _ctx.executeAgain();

            }
            else              // * Integrity check: failure
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: failure", _ctx._last_window, fcn, _ctx._lastTileSize);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                SPDLOG_DEBUG("Sending SCHC Compound ACK");

                /* Revisa si alguna ventana tiene tiles perdidos */
                std::vector<uint8_t> windows_with_error;
                for(int i=0; i < _ctx._last_window; i++)
                {
                    int c = this->get_c_from_bitmap(i); // Bien usado
                    if(c == 0)
                        windows_with_error.push_back(i);
                }
                windows_with_error.push_back(_ctx._last_window); // Dado que no hay como validar si la ultima ventana tiene error, se envia la ultima de todas formas

                SCHCGWMessage encoder; 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack_compound(_ctx._ruleID, _ctx._dTag, _ctx._last_window, windows_with_error, _ctx._bitmapArray, _ctx._windowSize); 

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, C=0 -------| {}", encoder.get_compound_bitmap_str());
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;

            }
 
        }
        else if(msg_type == SCHCMsgType::SCHC_ACK_REQ_MSG)
        {


            SPDLOG_DEBUG("Receiving SCHC ACK REQ");
            decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
            uint8_t w_received          = decoder.get_w();

            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|--- ACK REQ, W={:<1} -->| ", w_received);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

            if(w_received > _ctx._last_window)
                _ctx._last_window    = w_received;    // aseguro que el ultimo fragmento recibido va a marcar cual es la ultima ventana recibida


            bool rcs_result = this->check_rcs(_ctx._rcs);
            if(rcs_result)    // * Integrity check: success
            {
                SPDLOG_DEBUG("Sending SCHC Compound ACK");
                SCHCGWMessage encoder;
                std::vector<uint8_t> windows_with_error;   // vector vacío  
                std::vector<uint8_t> buffer         = encoder.create_schc_ack_compound(_ctx._ruleID, _ctx._dTag, _ctx._last_window, windows_with_error, _ctx._bitmapArray, _ctx._windowSize); 

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, C=1 -------| {}", encoder.get_compound_bitmap_str());
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_END");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_END;
                _ctx.executeAgain();

            }
            else                        // * Integrity check: failure
            {
                SPDLOG_DEBUG("Sending SCHC Compound ACK");
                /* Revisa si alguna ventana tiene tiles perdidos */
                std::vector<uint8_t> windows_with_error;
                for(int i=0; i < _ctx._last_window; i++)
                {
                    int c = this->get_c_from_bitmap(i); // Bien usado
                    if(c == 0)
                        windows_with_error.push_back(i);
                }
                windows_with_error.push_back(_ctx._last_window);                       

                SCHCGWMessage encoder; 
                std::vector<uint8_t> buffer         = encoder.create_schc_ack_compound(_ctx._ruleID, _ctx._dTag, _ctx._last_window, windows_with_error, _ctx._bitmapArray, _ctx._windowSize); 

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);


                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, C=0 -------| {}", encoder.get_compound_bitmap_str());
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_x_MISSING_FRAGS");
                _ctx._nextStateStr = SCHCAckOnErrorReceiverStates::STATE_WAIT_X_MISSING_FRAG;

            }
 

        }
    }




}

void SCHCAckOnErrorReceiver_RCV_WINDOW::timerExpired()
{
}

void SCHCAckOnErrorReceiver_RCV_WINDOW::release()
{
}

int SCHCAckOnErrorReceiver_RCV_WINDOW::get_tile_ptr(uint8_t window, uint8_t fcn)
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

int SCHCAckOnErrorReceiver_RCV_WINDOW::get_bitmap_ptr(uint8_t fcn)
{
    return (_ctx._windowSize - 1) - fcn;
}

uint8_t SCHCAckOnErrorReceiver_RCV_WINDOW::get_c_from_bitmap(uint8_t window)
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

std::vector<uint8_t> SCHCAckOnErrorReceiver_RCV_WINDOW::get_bitmap_array_vec(uint8_t window)
{
    std::vector<uint8_t> bitmap_v;
    for(int i=0; i<_ctx._windowSize; i++)
    {
        bitmap_v.push_back(_ctx._bitmapArray[window][i]);
    }

    return bitmap_v;
}

bool SCHCAckOnErrorReceiver_RCV_WINDOW::check_rcs(uint32_t rcs)
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

uint32_t SCHCAckOnErrorReceiver_RCV_WINDOW::calculate_crc32(const std::vector<uint8_t>& data) 
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

void SCHCAckOnErrorReceiver_RCV_WINDOW::print_bitmap_array_str()
{
    for(int i=0; i<_ctx._nMaxWindows; i++)
    {
        std::ostringstream oss;
        oss << "Bitmap window " + std::to_string(i) + ": " + get_bitmap_array_str(i);
        SPDLOG_INFO("{}", oss.str());
    }
}

std::string SCHCAckOnErrorReceiver_RCV_WINDOW::get_bitmap_array_str(uint8_t window)
{
    std::string bitmap_str = "";
    for(int i=0; i<_ctx._windowSize; i++)
    {
        bitmap_str = bitmap_str + std::to_string(_ctx._bitmapArray[window][i]);
    }
    return bitmap_str;
}