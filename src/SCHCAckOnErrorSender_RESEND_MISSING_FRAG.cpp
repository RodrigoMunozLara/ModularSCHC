#include "SCHCAckOnErrorSender_RESEND_MISSING_FRAG.hpp"
#include "SCHCAckOnErrorSender.hpp"

SCHCAckOnErrorSender_RESEND_MISSING_FRAG::SCHCAckOnErrorSender_RESEND_MISSING_FRAG(SCHCAckOnErrorSender& ctx) : _ctx(ctx)
{
}

SCHCAckOnErrorSender_RESEND_MISSING_FRAG::~SCHCAckOnErrorSender_RESEND_MISSING_FRAG()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorSender_RESEND_MISSING_FRAG destructor()");
}

void SCHCAckOnErrorSender_RESEND_MISSING_FRAG::execute(const std::vector<uint8_t>& msg)
{
    SCHCNodeMessage encoder;           // encoder

    // Buscar el primer cero y cuenta los ceros contiguos
    int adjacent_tiles      = 0;
    int bitmap_ptr          = -1;   // indice al primer zero del bitmap contando de izquierda a derecha
    int last_ptr            = -1;

    if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_WIN)
    {
        /* Se envía un SCHC ACK REQ para empujar 
        el envio en el downlink del SCHC ACK enviado 
        por el SCHC Gateway */
        // if(_ctx._send_schc_ack_req_flag == true)
        // {
        //     SPDLOG_DEBUG("SCHC ACK REQ sent to trigger the Gateway's SCHC ACK downlink");
        //     if((_ctx._currentWindow == _ctx._nWindows-1) && _ctx._bitmapArray[_ctx._currentWindow][_ctx._windowSize-1] == 0)
        //     {
        //         /* Sending a SCHC All-1 fragment */
        //         /* Crea un mensaje SCHC en formato hexadecimal */
        //         std::vector<uint8_t> schc_all_1_message = encoder.create_all_1_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._rcs, _ctx._lastTile);

        //         /* Imprime los mensajes para visualizacion ordenada */
        //         encoder.print_msg(SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message); 

        //         /* Envía el mensaje a la capa 2*/
        //         _ctx._stack->send_frame(_ctx._ruleID, schc_all_1_message);             
        //     }

        //     SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in false");
        //     _ctx._send_schc_ack_req_flag  = false;
        //     //_ctx._retrans_ack_req_flag   = true;

        //     SCHCNodeMessage encoder_2;

        //     /* Crea un mensaje SCHC en formato hexadecimal */
        //     std::vector<uint8_t>   schc_message = encoder_2.create_ack_request(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow);

        //     /* Imprime los mensajes para visualizacion ordenada */
        //     encoder_2.print_msg(SCHCMsgType::SCHC_ACK_REQ_MSG, schc_message); 

        //     /* Envía el mensaje a la capa 2*/
        //     _ctx._stack->send_frame(_ctx._ruleID, schc_message);

        //     SPDLOG_DEBUG("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_ACK");
        //     _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
        //     _ctx.executeTimer();

        //     return;         
        // }

        /* Determina cual es el ultimo tile de la ventana */
        if(_ctx._currentWindow == (_ctx._nWindows-1))
        {
            last_ptr =  _ctx._nFullTiles - (_ctx._nWindows - 1) * _ctx._windowSize;
        }
        else
        {
            last_ptr = _ctx._windowSize;
        }

        /* Cuenta el numero de tiles perdidos contiguos */
        for (int i = 0; i < last_ptr; ++i) 
        {
            if (_ctx._bitmapArray[_ctx._currentWindow][i] == 0) 
            {
                if (bitmap_ptr == -1) 
                {
                    bitmap_ptr = i;     // Registrar la posición del primer cero
                }
                ++adjacent_tiles;       // Contar los ceros contiguos
            } 
            else if (bitmap_ptr != -1) 
            {
                break;                  // Salir del bucle después de contar los ceros contiguos
            }
        }
        SPDLOG_DEBUG("Number of contiguous missing tiles: {}", adjacent_tiles);

        /* Numero de tiles que se pueden enviar un un payload */
        int payload_available_in_bytes = _ctx._current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
        int payload_available_in_tiles = payload_available_in_bytes/_ctx._tileSize;

        int n_tiles_to_send     = 0;    // numero de tiles a enviar
        if(adjacent_tiles > payload_available_in_tiles)
        {
            n_tiles_to_send = payload_available_in_tiles;
        }
        else
        {
            n_tiles_to_send = adjacent_tiles;
        }
            
        if(n_tiles_to_send != 0)
        {           
            int currentTile_ptr = getCurrentTile_ptr(_ctx._currentWindow, bitmap_ptr);
            int currentFcn      = get_current_fcn(bitmap_ptr);


            /* buffer que almacena todos los tiles que se van a enviar */           
            std::vector<uint8_t>   schc_payload = extractTiles(currentTile_ptr, n_tiles_to_send);

            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, currentFcn, schc_payload);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, schc_message);

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);


            /* Marca con 1 los tiles que se han retransmitido */
            for(int i = bitmap_ptr; i < (bitmap_ptr + n_tiles_to_send); i++)
            {
                _ctx._bitmapArray[_ctx._currentWindow][i] = 1;
            }


        }


        /* Revisa si hay mas tiles perdidos*/
        int ones = 0;
        for(int i = 0; i<last_ptr; i++)
        {
            if(_ctx._bitmapArray[_ctx._currentWindow][i] == 0)
            {
                SPDLOG_DEBUG("There are more tiles to send.");
                //SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in false");
                //_ctx._send_schc_ack_req_flag = false;
                _ctx.executeAgain();
                return;
            }
            else
            {
                ones = ones + _ctx._bitmapArray[_ctx._currentWindow][i];
            }

            if(ones == last_ptr)
            {
                SPDLOG_DEBUG("There are no more tiles to send.");
                //SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in true");
                //_ctx._send_schc_ack_req_flag = true;

                SPDLOG_DEBUG("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_ACK");
                _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
                _ctx.executeTimer();
                return;
            }
        }

    }
    else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_SES)
    {
        // if(_ctx._send_schc_ack_req_flag == true)
        // {

        //     if((_ctx._last_confirmed_window == _ctx._nWindows-1) && _ctx._bitmapArray[_ctx._currentWindow][_ctx._windowSize-1] == 0)
        //     {
        //         /* Sending a SCHC All-1 fragment */
        //         SCHCNodeMessage    encoder_all_1;

        //         /* Crea un mensaje SCHC en formato hexadecimal */
        //         std::vector<uint8_t> schc_all_1_message = encoder_all_1.create_all_1_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._rcs, _ctx._lastTile);

        //         /* Imprime los mensajes para visualizacion ordenada */
        //         encoder_all_1.print_msg(SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message); 

        //         /* Envía el mensaje a la capa 2*/
        //         _ctx._stack->send_frame(_ctx._ruleID, schc_all_1_message);                 
        //     }



        //     /* Se envía un SCHC ACK REQ para empujar el envio en el downlink
        //     del SCHC ACK enviado por el SCHC Gateway */
        //     SCHCNodeMessage encoder_ack_req;

        //     /* Crea un mensaje SCHC en formato hexadecimal */
        //     std::vector<uint8_t>   schc_message = encoder_ack_req.create_ack_request(_ctx._ruleID, _ctx._dTag, _ctx._last_confirmed_window);

        //     /* Imprime los mensajes para visualizacion ordenada */
        //     encoder_ack_req.print_msg(SCHCMsgType::SCHC_ACK_REQ_MSG, schc_message); 

        //     /* Envía el mensaje a la capa 2*/
        //     _ctx._stack->send_frame(_ctx._ruleID, schc_message);

        //     //_ctx._retrans_ack_req_flag       = true;
        //     SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in false");
        //     _ctx._send_schc_ack_req_flag     = false;

            
        //     SPDLOG_DEBUG("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_ACK");
        //     _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
        //     _ctx.executeTimer();

        //     return;            
        // }


        /* Determina cual es el ultimo tile de la ventana */
        if(_ctx._last_confirmed_window == (_ctx._nWindows-1))
        {
            last_ptr =  _ctx._nFullTiles - (_ctx._nWindows - 1) * _ctx._windowSize;
        }
        else
        {
            last_ptr = _ctx._windowSize;
        } 

        /* Cuenta el numero de tiles perdidos contiguos */
        SPDLOG_DEBUG("Counting the number of contiguous missing tiles");
        for (int i = 0; i < last_ptr; ++i) 
        {
            if (_ctx._bitmapArray[_ctx._last_confirmed_window][i] == 0) 
            {
                if (bitmap_ptr == -1) 
                {
                    bitmap_ptr = i;     // Registrar la posición del primer cero
                }
                ++adjacent_tiles;       // Contar los ceros contiguos
            } 
            else if (bitmap_ptr != -1) 
            {
                break;                  // Salir del bucle después de contar los ceros contiguos
            }
        }


        /* Numero de tiles que se pueden enviar un un payload */
        int payload_available_in_bytes = _ctx._current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
        int payload_available_in_tiles = payload_available_in_bytes/_ctx._tileSize;

        int n_tiles_to_send     = 0;    // numero de tiles a enviar
        if(adjacent_tiles > payload_available_in_tiles)
        {
            n_tiles_to_send = payload_available_in_tiles;
        }
        else
        {
            n_tiles_to_send = adjacent_tiles;
        }

        if(n_tiles_to_send != 0)
        {

            /* buffer que almacena todos los tiles que se van a enviar */      
            int currentTile_ptr = getCurrentTile_ptr(_ctx._last_confirmed_window, bitmap_ptr);
            int currentFcn      = get_current_fcn(bitmap_ptr);

            /* buffer que almacena todos los tiles que se van a enviar */           
            std::vector<uint8_t>   schc_payload = extractTiles(_ctx._currentTile_ptr, n_tiles_to_send);

            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._currentFcn, schc_payload);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, schc_message);

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);

            /* Marca con 1 los tiles que se han retransmitido */
            for(int i = bitmap_ptr; i < (bitmap_ptr + n_tiles_to_send); i++)
            {
                _ctx._bitmapArray[_ctx._last_confirmed_window][i] = 1;
            }

        }

        /* Revisa si hay tiles perdidos. Si los hay, vuelve a llamar a este mismo metodo*/
        int c = 1;
        for(int i = 0; i<last_ptr; i++)
        {
            if(_ctx._bitmapArray[_ctx._last_confirmed_window][i] == 0)
            {
                c = 0;
                break;
            }
        }

        if(c == 1)
        {
            /* NO  hay tiles perdidos. Se marca flag en true y se vuelve a llamar este mismo metodo con para enviar un SCHC ACK REQ*/
            SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in true");
            _ctx._send_schc_ack_req_flag = true;
        }
    }
    else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_COMPOUND)
    {

        /* Extrae la ventana a la que se deben enviar los tails*/
        if(!_ctx._win_with_errors.empty())
            _ctx._last_confirmed_window = _ctx._win_with_errors.front();

        /* Se envía un SCHC ACK REQ para empujar el envio en 
        el downlink del SCHC ACK enviado por el SCHC Gateway */
        if(_ctx._send_schc_ack_req_flag == true)
        {

            if((_ctx._last_confirmed_window == _ctx._nWindows-1) && _ctx._bitmapArray[_ctx._currentWindow][_ctx._windowSize-1] == 0)
            {
                /* Sending a SCHC All-1 fragment */
                SCHCNodeMessage    encoder_all_1;

                /* Crea un mensaje SCHC en formato hexadecimal */
                std::vector<uint8_t> schc_all_1_message = encoder_all_1.create_all_1_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._rcs, _ctx._lastTile);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder_all_1.print_msg(SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message); 

                /* Envía el mensaje a la capa 2*/
                _ctx._stack->send_frame(_ctx._ruleID, schc_all_1_message);                
            }
         

            SCHCNodeMessage encoder_2;

            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder_2.create_ack_request(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder_2.print_msg(SCHCMsgType::SCHC_ACK_REQ_MSG, schc_message); 

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);

            SPDLOG_DEBUG("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_ACK");
            _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
            _ctx.executeTimer();

            //_ctx._retrans_ack_req_flag       = true;
            SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in false");
            _ctx._send_schc_ack_req_flag     = false;

            return;            
        }

        /* Determina cual es el ultimo tile de la ventana */
        if(_ctx._last_confirmed_window == (_ctx._nWindows-1))
        {
            last_ptr =  _ctx._nFullTiles - (_ctx._nWindows - 1) * _ctx._windowSize;
        }
        else
        {
            last_ptr = _ctx._windowSize;
        } 

       /* Cuenta el numero de tiles perdidos contiguos */
        for (int i = 0; i < last_ptr; ++i) 
        {
            if (_ctx._bitmapArray[_ctx._last_confirmed_window][i] == 0) 
            {
                if (bitmap_ptr == -1) 
                {
                    bitmap_ptr = i;     // Registrar la posición del primer cero
                }
                ++adjacent_tiles;       // Contar los ceros contiguos
            } 
            else if (bitmap_ptr != -1) 
            {
                break;                  // Salir del bucle después de contar los ceros contiguos
            }
        }


        /* Numero de tiles que se pueden enviar un un payload */
        int payload_available_in_bytes = _ctx._current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
        int payload_available_in_tiles = payload_available_in_bytes/_ctx._tileSize;

        int n_tiles_to_send     = 0;    // numero de tiles a enviar
        if(adjacent_tiles > payload_available_in_tiles)
        {
            n_tiles_to_send = payload_available_in_tiles;
        }
        else
        {
            n_tiles_to_send = adjacent_tiles;
        }

         if(n_tiles_to_send != 0)
         {             
            int currentTile_ptr = getCurrentTile_ptr(_ctx._last_confirmed_window, bitmap_ptr);
            int currentFcn      = get_current_fcn(bitmap_ptr);

            /* buffer que almacena todos los tiles que se van a enviar */           
            std::vector<uint8_t>   schc_payload = extractTiles(_ctx._currentTile_ptr, n_tiles_to_send);

            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._last_confirmed_window, _ctx._currentFcn, schc_payload);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, schc_message);

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);

            /* Marca con 1 los tiles que se han retransmitido */
            for(int i = bitmap_ptr; i < (bitmap_ptr + n_tiles_to_send); i++)
            {
               _ctx._bitmapArray[_ctx._last_confirmed_window][i] = 1;
            }
        }

        /* Revisa si hay mas tiles perdidos. Si los hay, vuelve a llamar a este mismo metodo*/
        int c = 1;
        for(int i = 0; i<last_ptr; i++)
        {
            if(_ctx._bitmapArray[_ctx._last_confirmed_window][i] == 0)
            {
                c = 0;
                break;
            }
        }

        if(c == 1)
        {
            /* No hay mas tiles perdidos en la ventana actual.
            Se elimina la ventana corregida del vector _win_with_errors. 
            Se vuelve a llamar a este mismo metodo */
            if(!_ctx._win_with_errors.empty())
                _ctx._win_with_errors.erase(_ctx._win_with_errors.begin());

            if(_ctx._win_with_errors.empty())
            {
                SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in true");
                _ctx._send_schc_ack_req_flag = true;
            }

        }
    }

}

void SCHCAckOnErrorSender_RESEND_MISSING_FRAG::timerExpired()
{
}

void SCHCAckOnErrorSender_RESEND_MISSING_FRAG::release()
{
}

int SCHCAckOnErrorSender_RESEND_MISSING_FRAG::getCurrentTile_ptr(int window, int bitmap_ptr)
{
    return (window * _ctx._windowSize) + bitmap_ptr;
}

uint8_t SCHCAckOnErrorSender_RESEND_MISSING_FRAG::get_current_fcn(int bitmap_ptr)
{
    return (_ctx._windowSize - 1) - bitmap_ptr;
}

std::vector<uint8_t> SCHCAckOnErrorSender_RESEND_MISSING_FRAG::extractTiles(uint8_t firstTileID, uint8_t nTiles)
{
    // 1. Calculamos el tamaño total necesario
    size_t totalSize = nTiles * _ctx._tileSize;
    
    // 2. Creamos el vector resultado y reservamos memoria de golpe
    std::vector<uint8_t> buffer;
    buffer.reserve(totalSize); 

    // 3. Copiamos las "tiles" una a una
    // Usamos un rango basado en los IDs proporcionados
    for (int i = firstTileID; i < (firstTileID + nTiles); ++i)
    {
        // Insertamos el contenido completo de la sub-vector (tile) al final del buffer
        buffer.insert(buffer.end(), _ctx._tilesArray[i].begin(), _ctx._tilesArray[i].end());
    }

    return buffer;
}