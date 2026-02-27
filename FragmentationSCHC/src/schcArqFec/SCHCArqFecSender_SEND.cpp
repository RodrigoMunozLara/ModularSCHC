#include "schcArqFec/SCHCArqFecSender.hpp"
#include "schcArqFec/SCHCArqFecSender_SEND.hpp"
#include <schcAckOnError/SCHCNodeMessage.hpp>


SCHCArqFecSender_SEND::SCHCArqFecSender_SEND(SCHCArqFecSender& ctx): _ctx(ctx)
{

}

SCHCArqFecSender_SEND::~SCHCArqFecSender_SEND()
{
    SPDLOG_DEBUG("Executing SCHCArqFecSender_SEND destructor()");
}

void SCHCArqFecSender_SEND::execute(const std::vector<uint8_t>& msg)
{
    if(!msg.empty())
    {
        SCHCNodeMessage decoder;
        SPDLOG_DEBUG("Decoding Message...");
        SCHCMsgType msg_type = decoder.get_msg_type(ProtocolType::LORAWAN, _ctx._ruleID, msg);
        
        if(msg_type == SCHCMsgType::SCHC_ACK_MSG)
        {
            SPDLOG_DEBUG("Receiving a SCHC ACK msg");

            SPDLOG_DEBUG("Stoping the S timer...");
            _ctx._timer.stop();

            decoder.decodeMsg(ProtocolType::LORAWAN, _ctx._ruleID, msg, SCHCAckMechanism::ARQ_FEC, &(_ctx._bitmapArray));
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            decoder.print_msg(SCHCMsgType::SCHC_ACK_MSG, msg, _ctx._bitmapArray);

            if(c == 1 && w == 1)
            {
                SCHCNodeMessage encoder;

                /* Crea un mensaje SCHC en formato hexadecimal */
                std::vector<uint8_t> schc_all_1_message = encoder.create_all_1_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._rcs, _ctx._lastTile);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder.print_msg(SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message); 

                /* Envía el mensaje a la capa 2*/
                _ctx._stack->send_frame(_ctx._ruleID, schc_all_1_message);


                SPDLOG_DEBUG("Changing STATE: From STATE_TX_SEND --> STATE_TX_WAIT_x_SESSION_ACK");
                _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_WAIT_x_SESSION_ACK;
                _ctx.executeTimer(5);
                return;
                
            }
            else
            {
                SPDLOG_ERROR("The SCHC ACK message must have the following parameters (C=1 and W=1)");     
                return;
            }
        }
        else
        {
            SPDLOG_WARN("Only SCHC ACKs are permitted to be received");
            return;
        }
    }
    else
    {
        /* Number of tiles that can be sent in a payload */
        int payload_available_in_bytes = _ctx._current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
        int payload_available_in_tiles = payload_available_in_bytes/_ctx._tileSize;

        /* Temporary variables */
        int n_tiles_to_send     = 0;    // number of tiles to send
        int n_remaining_tiles   = 0;    // Number of remaining tiles to send (used in session confirmation mode)
    
        SCHCNodeMessage encoder;        // encoder 

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


            SPDLOG_DEBUG("Changing STATE: From STATE_TX_SEND --> STATE_WAIT_x_SESSION_ACK");
            _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_WAIT_x_SESSION_ACK;
            _ctx.executeTimer(_ctx._retransTimer); 
        } 
    }




}

void SCHCArqFecSender_SEND::timerExpired()
{
}

void SCHCArqFecSender_SEND::release()
{
}

std::vector<uint8_t> SCHCArqFecSender_SEND::extractTiles(uint8_t firstTileID, uint8_t nTiles)
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

