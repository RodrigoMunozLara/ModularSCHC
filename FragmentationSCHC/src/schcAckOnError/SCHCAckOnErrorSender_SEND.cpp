#include "schcAckOnError/SCHCAckOnErrorSender_SEND.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender.hpp"


SCHCAckOnErrorSender_SEND::SCHCAckOnErrorSender_SEND(SCHCAckOnErrorSender& ctx): _ctx(ctx)
{
}

SCHCAckOnErrorSender_SEND::~SCHCAckOnErrorSender_SEND()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorSender_SEND destructor()");
}

void SCHCAckOnErrorSender_SEND::execute(const std::vector<uint8_t>& msg)
{
    /* Number of tiles that can be sent in a payload */
    int payload_available_in_bytes = _ctx._current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
    int payload_available_in_tiles = payload_available_in_bytes/_ctx._tileSize;

    /* Temporary variables */
    int n_tiles_to_send     = 0;    // number of tiles to send
    int n_remaining_tiles   = 0;    // Number of remaining tiles to send (used in session confirmation mode)
   
    SCHCNodeMessage encoder;        // encoder 

    if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_WIN)
    {
        /* Determine the number of tiles remaining to be sent.*/
        if(_ctx._currentWindow == (_ctx._nWindows - 1))
        {
            n_remaining_tiles = _ctx._nFullTiles - _ctx._currentTile_ptr;
        }     
        else
        {
            n_remaining_tiles = _ctx._currentFcn + 1;
        }
            
        if(n_remaining_tiles>payload_available_in_tiles)
        {
            /* If the number of tiles remaining to be sent 
            * for the i-th window does not reach the MTU, 
            * it means that the i-th window is NOT yet complete.
            */

            n_tiles_to_send = payload_available_in_tiles;

            /* buffer que almacena todos los tiles que se van a enviar */           
            std::vector<uint8_t>   schc_payload = extractTiles(_ctx._currentTile_ptr, n_tiles_to_send);

            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._currentFcn, schc_payload);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, schc_message);

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);

            
            _ctx._currentTile_ptr    = _ctx._currentTile_ptr + n_tiles_to_send;
            _ctx._currentFcn         = _ctx._currentFcn - n_tiles_to_send;

            _ctx.executeAgain();
            return;
        }
        else
        {
            /* If the number of tiles remaining to be sent for the 
            i-th window reaches the MTU, this means that this is 
            the last transmission for the i-th window. */

            n_tiles_to_send = n_remaining_tiles;

            /* vector que almacena todos los tiles que se van a enviar */ 
            std::vector<uint8_t>   schc_payload = extractTiles(_ctx._currentTile_ptr, n_tiles_to_send);

            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._currentFcn, schc_payload);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, schc_message); 

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);
            

            _ctx._currentTile_ptr        = _ctx._currentTile_ptr + n_tiles_to_send;
            _ctx._currentFcn             = (_ctx._windowSize)-1;
            

            /* ******************* SCHC ALL-1 *********************************** */
            /* If all full tiles from the session have been sent, then an All-1 must be sent.*/
            if(_ctx._currentWindow == (_ctx._nWindows - 1))
            {     
                /* ******************* SCHC ALL-1 ********************************* */    
                /* Crea un mensaje SCHC en formato hexadecimal */
                std::vector<uint8_t> schc_all_1_message = encoder.create_all_1_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._rcs, _ctx._lastTile);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder.print_msg(SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message); 

                /* Envía el mensaje a la capa 2*/
                _ctx._stack->send_frame(_ctx._ruleID, schc_all_1_message);
    
            } 

            SPDLOG_DEBUG("Changing STATE: From STATE_TX_SEND --> STATE_TX_WAIT_x_ACK");
            _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
            _ctx.executeTimer(_ctx._retransTimer);

            return;
        }
        
    }
    else if(_ctx._ackMechanism == SCHCAckMechanism::ACK_END_SES || _ctx._ackMechanism == SCHCAckMechanism::ACK_COMPOUND)
    {
        if(_ctx._send_schc_ack_req_flag == true)
        {
            /* ******************* Enviando SCHC ACK REQ ***************************** */
            /* Se envía un SCHC ACK REQ para empujar el envio en el downlink
            del SCHC ACK enviado por el SCHC Gateway */
            SCHCNodeMessage encoder;
                 
            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_ack_request(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHCMsgType::SCHC_ACK_REQ_MSG, schc_message); 

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);
            
            
            //_ctx._retrans_ack_req_flag   = true;
            SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in false");
            _ctx._send_schc_ack_req_flag = false;


            SPDLOG_DEBUG("Changing STATE: From STATE_TX_SEND --> STATE_TX_WAIT_x_ACK");
            _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
            _ctx.executeTimer(_ctx._retransTimer);

            
            return;
        }

        n_remaining_tiles = _ctx._nFullTiles - _ctx._currentTile_ptr; // numero de tiles de toda la sesion que faltan por enviar
 
        if(n_remaining_tiles > payload_available_in_tiles)
        {
            /* Si la cantidad de tiles que faltan por enviar 
            para la sesion no alcanza en la MTU significa 
            que aun NO finaliza la sesion
            */
            n_tiles_to_send = payload_available_in_tiles;

            /* buffer que almacena todos los tiles que se van a enviar */           
            std::vector<uint8_t>   schc_payload = extractTiles(_ctx._currentTile_ptr, n_tiles_to_send);

            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._currentFcn, schc_payload);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, schc_message);

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);

            _ctx._currentTile_ptr    = _ctx._currentTile_ptr + n_tiles_to_send;

            
            if((_ctx._currentFcn - n_tiles_to_send <= -1))   // * detecta si se comenzaron a enviar los tiles de la siguiente ventana
            {
                _ctx._currentFcn = (_ctx._currentFcn - n_tiles_to_send) + _ctx._windowSize;
                _ctx._currentWindow = _ctx._currentWindow + 1;
            }
            else
            {
                _ctx._currentFcn = _ctx._currentFcn - n_tiles_to_send;
            }

            _ctx.executeAgain();
            return;

        }
        else
        {
           /* Si la cantidad de tiles que faltan por enviar 
            para la sesion SI alcanza en la MTU 
            significa que este es el ultimo envio para la 
            sesion
            */
            n_tiles_to_send = n_remaining_tiles;

            /* vector que almacena todos los tiles que se van a enviar */ 
            std::vector<uint8_t>   schc_payload = extractTiles(_ctx._currentTile_ptr, n_tiles_to_send);


            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._currentFcn, schc_payload);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, schc_message); 

            /* Envía el mensaje a la capa 2*/
            _ctx._stack->send_frame(_ctx._ruleID, schc_message);
            
            _ctx._currentTile_ptr    = _ctx._currentTile_ptr + n_tiles_to_send;
     
            /* ******************* SCHC ALL-1 *********************************** */
            /* Se envia el ultimo tile en un SCHC All-1 */
            if(_ctx._currentWindow == (_ctx._nWindows - 1))
            {
                /* Crea un mensaje SCHC en formato hexadecimal */
                std::vector<uint8_t> schc_all_1_message = encoder.create_all_1_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._rcs, _ctx._lastTile);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder.print_msg(SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message); 

                /* Envía el mensaje a la capa 2*/
                _ctx._stack->send_frame(_ctx._ruleID, schc_all_1_message);
 
            }

 
            SPDLOG_DEBUG("Changing STATE: From STATE_TX_SEND --> STATE_TX_WAIT_x_ACK");
            _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_WAIT_x_ACK;
            _ctx.executeTimer(_ctx._retransTimer); 

            SPDLOG_DEBUG("Setting _send_schc_ack_req_flag in true");
            _ctx._send_schc_ack_req_flag = true;
        }        
    }


}

void SCHCAckOnErrorSender_SEND::timerExpired()
{
}

void SCHCAckOnErrorSender_SEND::release()
{
}

std::vector<uint8_t> SCHCAckOnErrorSender_SEND::extractTiles(uint8_t firstTileID, uint8_t nTiles)
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
