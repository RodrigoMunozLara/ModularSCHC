#include "schcArqFec/SCHCArqFecSender.hpp"
#include "schcArqFec/SCHCArqFecSender_WAIT_X_S_ACK.hpp"
#include <schcAckOnError/SCHCNodeMessage.hpp>


SCHCArqFecSender_WAIT_X_S_ACK::SCHCArqFecSender_WAIT_X_S_ACK(SCHCArqFecSender& ctx): _ctx(ctx)
{

}

SCHCArqFecSender_WAIT_X_S_ACK::~SCHCArqFecSender_WAIT_X_S_ACK()
{
    SPDLOG_DEBUG("Executing SCHCArqFecSender_WAIT_X_S_ACK destructor()");
}

void SCHCArqFecSender_WAIT_X_S_ACK::execute(const std::vector<uint8_t>& msg)
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

        if(c == 1 && w == 0)
        {
            

            SPDLOG_DEBUG("Changing STATE: From STATE_TX_WAIT_x_S_ACK --> STATE_TX_SEND");
            _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_SEND;
            _ctx.executeAgain();
            
        }
        else
        {
            SPDLOG_ERROR("It is not possible to receive an SCHC ACK with a C = 0.");     
        }
    }
    else
    {
        SPDLOG_WARN("Only SCHC ACKs are permitted to be received.");
    }

}

void SCHCArqFecSender_WAIT_X_S_ACK::timerExpired()
{
    SCHCNodeMessage encoder;        // encoder 

    /* Imprime los mensajes para visualizacion ordenada */
    encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, _ctx._first_fragment_msg);

    /* Envía el mensaje a la capa 2*/
    _ctx._stack->send_frame(_ctx._ruleID, _ctx._first_fragment_msg);

    SPDLOG_DEBUG("Setting S-timer: {} seconds", _ctx._sTimer);
    _ctx.executeTimer(_ctx._sTimer);

    _ctx.executeAgain();
}

void SCHCArqFecSender_WAIT_X_S_ACK::release()
{
}

uint32_t SCHCArqFecSender_WAIT_X_S_ACK::calculate_crc32(const std::vector<uint8_t>& msg) 
{
    // CRC32 polynomial (mirrored)
    const uint32_t polynomial = 0xEDB88320;
    uint32_t crc = 0xFFFFFFFF;

    // Process each byte in the buffer
    for (size_t i = 0; i < msg.size(); i++) {
        crc ^= msg[i]; // Ensure that the data is treated as uint8_t.

        // Process the 8 bits of the byte
        for (int j = 0; j < 8; j++) {
            if (crc & 1) 
            {
                crc = (crc >> 1) ^ polynomial;
            } 
            else 
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}

std::vector<uint8_t> SCHCArqFecSender_WAIT_X_S_ACK::extractTiles(uint8_t firstTileID, uint8_t nTiles)
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
