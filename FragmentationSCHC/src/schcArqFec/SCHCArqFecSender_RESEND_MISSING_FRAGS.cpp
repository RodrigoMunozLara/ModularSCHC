#include "schcArqFec/SCHCArqFecSender.hpp"
#include "schcArqFec/SCHCArqFecSender_RESEND_MISSING_FRAGS.hpp"
#include <schcAckOnError/SCHCNodeMessage.hpp>


SCHCArqFecSender_RESEND_MISSING_FRAGS::SCHCArqFecSender_RESEND_MISSING_FRAGS(SCHCArqFecSender& ctx): _ctx(ctx)
{

}

SCHCArqFecSender_RESEND_MISSING_FRAGS::~SCHCArqFecSender_RESEND_MISSING_FRAGS()
{
    SPDLOG_DEBUG("Executing SCHCArqFecSender_RESEND_MISSING_FRAGS destructor()");
}

void SCHCArqFecSender_RESEND_MISSING_FRAGS::execute(const std::vector<uint8_t>& msg)
{
    if(!msg.empty())
    {
        SCHCNodeMessage decoder;
        SPDLOG_DEBUG("Decoding Message...");
        SCHCMsgType msg_type = decoder.get_msg_type(_ctx._protoType, _ctx._ruleID, msg);

        if(msg_type == SCHCMsgType::SCHC_ACK_MSG)
        {
            SPDLOG_DEBUG("Receiving a SCHC ACK");

            SPDLOG_DEBUG("Stoping the Retransmission timer...");
            _ctx._timer.stop();

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
            }
        }
    }
    else
    {
        SCHCNodeMessage encoder;           // encoder

        // Buscar el primer cero y cuenta los ceros contiguos
        int adjacent_tiles      = 0;
        int bitmap_ptr          = -1;   // indice al primer zero del bitmap contando de izquierda a derecha
        int last_ptr            = -1;

        /* Extrae la ventana a la que se deben enviar los tails*/
        if(!_ctx._win_with_errors.empty())
        {
            _ctx._last_confirmed_window = _ctx._win_with_errors.front();
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
            std::vector<uint8_t>   schc_payload = extractTiles(currentTile_ptr, n_tiles_to_send);

            /* Crea un mensaje SCHC en formato hexadecimal */
            std::vector<uint8_t>   schc_message = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._last_confirmed_window, currentFcn, schc_payload);

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
        for(int i = 0; i<last_ptr; i++)
        {
            if(_ctx._bitmapArray[_ctx._last_confirmed_window][i] == 0)
            {
                SPDLOG_DEBUG("There are more missing tiles in the window: {}. Calling to ExecuteAgain()", _ctx._last_confirmed_window);
                _ctx.executeAgain();
                return;
            }
        }

        /* No hay mas tiles perdidos en la ventana actual.
        Se elimina la ventana corregida del vector _win_with_errors. 
        Se vuelve a llamar a este mismo metodo */
        if(!_ctx._win_with_errors.empty())
        {
            _ctx._win_with_errors.erase(_ctx._win_with_errors.begin());

            if(_ctx._win_with_errors.empty())
            {
           
                SCHCNodeMessage encoder;
                    
                /* Crea un mensaje SCHC en formato hexadecimal */
                std::vector<uint8_t>   schc_message = encoder.create_ack_request(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder.print_msg(SCHCMsgType::SCHC_ACK_REQ_MSG, schc_message); 

                /* Envía el mensaje a la capa 2*/
                _ctx._stack->send_frame(_ctx._ruleID, schc_message);

                SPDLOG_DEBUG("There are no more windows with missing tiles.");
                SPDLOG_DEBUG("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_SESSION_ACK");
                _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_WAIT_x_SESSION_ACK;

                _ctx.executeTimer(_ctx._retransTimer);
                return;
            }
            else
            {
                SPDLOG_DEBUG("There are more windows with missing tiles. Calling executeAgain()");
                _ctx.executeAgain();
                return;
            }
        }


    }




}

void SCHCArqFecSender_RESEND_MISSING_FRAGS::timerExpired()
{
}

void SCHCArqFecSender_RESEND_MISSING_FRAGS::release()
{
}

std::vector<uint8_t> SCHCArqFecSender_RESEND_MISSING_FRAGS::extractTiles(uint8_t firstTileID, uint8_t nTiles)
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

int SCHCArqFecSender_RESEND_MISSING_FRAGS::getCurrentTile_ptr(int window, int bitmap_ptr)
{
    return (window * _ctx._windowSize) + bitmap_ptr;
}

uint8_t SCHCArqFecSender_RESEND_MISSING_FRAGS::get_current_fcn(int bitmap_ptr)
{
    return (_ctx._windowSize - 1) - bitmap_ptr;
}

