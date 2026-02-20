#include "schcAckOnError/SCHCNodeMessage.hpp"


SCHCNodeMessage::SCHCNodeMessage()
{

}

SCHCNodeMessage::~SCHCNodeMessage()
{
    SPDLOG_DEBUG("Executing SCHCNodeMessage destructor()");
}

std::vector<uint8_t> SCHCNodeMessage::create_regular_fragment(uint8_t ruleID, uint8_t dtag, uint8_t w, uint8_t fcn, const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> buffer;
    buffer.resize(payload.size() + 1);

    /* Mask definition */ 
    uint8_t w_mask = 0xC0;
    uint8_t fcn_mask = 0x3F;
    //byte c_mask = 0x20;

    /* SCHC header construction */
    uint8_t new_w = (w << 6) & w_mask;
    uint8_t new_fcn = (fcn & fcn_mask);
    uint8_t header = new_w | new_fcn;
    buffer[0] = header;

    std::copy(payload.begin(), payload.end(), buffer.begin() + 1);

    return buffer;
}

std::vector<uint8_t> SCHCNodeMessage::create_ack_request(uint8_t ruleID, uint8_t dtag, uint8_t w)
{
    std::vector<uint8_t> buffer;
    buffer.resize(1);

    /* Mask definition */ 
    uint8_t w_mask = 0xC0;
    //byte fcn_mask = 0x3F;
    //byte c_mask = 0x20;

    /* SCHC header construction */
    uint8_t new_w = (w << 6) & w_mask;
    uint8_t new_fcn = 0x00;
    uint8_t header = new_w | new_fcn;
    buffer[0] = header;

    return buffer;
}

std::vector<uint8_t> SCHCNodeMessage::create_sender_abort(uint8_t ruleID, uint8_t dtag, uint8_t w)
{
    std::vector<uint8_t> buffer;
    buffer.resize(1);

    /* Mask definition */ 
    uint8_t w_mask = 0xC0;
    //byte fcn_mask = 0x3F;
    //byte c_mask = 0x20;

    /* SCHC header construction */
    uint8_t new_w = (w << 6) & w_mask;
    uint8_t new_fcn = 0x3F;
    uint8_t header = new_w | new_fcn;
    buffer[0] = header;

    return buffer;
}

std::vector<uint8_t> SCHCNodeMessage::create_all_1_fragment(uint8_t ruleID, uint8_t dtag, uint8_t w, uint32_t rcs, const std::vector<uint8_t>& payload)
{
    size_t header_size = 1; 
    size_t rcs_size = 4; // LoRaWAN suele usar 4 bytes para el RCS
    size_t total_size = header_size + rcs_size + payload.size();

    std::vector<uint8_t> buffer;
    buffer.resize(total_size);

    /* SCHC header construction. byte 1 */
    uint8_t w_mask = 0xC0;
    uint8_t new_w = (w << 6) & w_mask;
    uint8_t new_fcn = 0x3F;
    uint8_t header = new_w | new_fcn;
    buffer[0] = header;

    /* SCHC header construction. byte 2 al byte 5 */
    buffer[1] = (rcs >> 24) & 0xFF; // Byte más significativo
    buffer[2] = (rcs >> 16) & 0xFF;
    buffer[3] = (rcs >> 8) & 0xFF;
    buffer[4] = rcs & 0xFF;         // Byte menos significativo

    std::copy(payload.begin(), payload.end(), buffer.begin() + 5);

    return buffer;
}

SCHCMsgType SCHCNodeMessage::get_msg_type(ProtocolType protocol, int rule_id, const std::vector<uint8_t>& msg)
{
    
    if(protocol==ProtocolType::LORAWAN)
    {
        int len = msg.size();
        uint8_t schc_header = msg[0];
        uint8_t _c = (schc_header >> 5) & 0x01;

        SCHCLoRaWANFragRule         _rule_id;
        if(rule_id == 20) _rule_id = SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID;
        else if (rule_id == 21) _rule_id = SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID;


        if(_rule_id == SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID && _c==1 && len==2)
            _msg_type = SCHCMsgType::SCHC_RECEIVER_ABORT_MSG;
        else if(_rule_id == SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID && _c==1)
            _msg_type = SCHCMsgType::SCHC_ACK_MSG;
        else if(_rule_id == SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID && _c==0 && len>9)
            _msg_type = SCHCMsgType::SCHC_COMPOUND_ACK;
        else if(_rule_id == SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID && _c==0)     
            _msg_type = SCHCMsgType::SCHC_ACK_MSG;
    }

    return _msg_type;
}

uint8_t SCHCNodeMessage::decodeMsg(ProtocolType protocol, int rule_id, const std::vector<uint8_t>& msg, SCHCAckMechanism ack_type, std::vector<std::vector<uint8_t>>* bitmap_array)
{
    if(protocol==ProtocolType::LORAWAN)
    {
        int len = msg.size();

        SCHCLoRaWANFragRule         _rule_id;
        if(rule_id == 20) _rule_id = SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID;
        else if (rule_id == 21) _rule_id = SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID;

        uint8_t schc_header = msg[0];
        _c = (schc_header >> 5) & 0x01;
        _w = (schc_header >> 6) & 0x03;

        if(_rule_id==SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID && _c==1 && len==2 && msg[1] == 0xFF)
        {
            // TODO: Se ha recibido un SCHC Receiver-Abort. No hacer nada.
        }
        else if(_rule_id==SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID && _c==1)
        {
            // * Se ha recibido un SCHC ACK (sin errores)
            SPDLOG_DEBUG("Receiving a SCHC ACK without errors");
            for(int i=0; i<63; i++)
            {
                (*bitmap_array)[_w][i] = 1;
            }
        }
        // else if(_rule_id==SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID && _c==0 && len>9)
        // {
        //     // * Se ha recibido un SCHC Compound ACK (con errores)
        //     SPDLOG_DEBUG("Receiving a SCHC Compound ACK with errors");
        //     int n_total_bits    = len*8;                    // en bits
        //     int n_win           = ceil((n_total_bits - 1)/65);     // window_size + M = 65. Se resta un bit a len debido al bit C.
        //     //int n_padding_bits  = n_total_bits - 1 - n_win*65;
        //     bool first_win      = true;

        //     std::vector<uint8_t> bitVector;
            
        //     for (int i = 0; i < len; ++i)
        //     {
        //         for (int j = 7; j >= 0; --j)
        //         {
        //             bitVector.push_back((msg[i] >> j) & 1);
        //         }
        //     }

        //     for(int i=0; i<n_win; i++)
        //     {
        //         if(first_win)
        //         {
        //             _windows_with_error.push_back(_w);      // almacena en el vector el numero de la primera ventana con error en el SCHC Compound ACK
                    
        //             bitVector.erase(bitVector.begin(), bitVector.begin()+3); // Se elimina del vector la ventana (2 bits) y c (1 bit)
        //             std::copy(bitVector.begin(), bitVector.begin() + 63, (*bitmap_array)[_w].begin());
        //             bitVector.erase(bitVector.begin(), bitVector.begin()+63);
        //             first_win = false;
        //         }
        //         else
        //         {
        //             uint8_t win = (bitVector[0] << 1) | bitVector[1];
        //             _windows_with_error.push_back(win);
        //             bitVector.erase(bitVector.begin(), bitVector.begin()+2);
        //             std::copy(bitVector.begin(), bitVector.begin() + 63, (*bitmap_array)[win].begin());
        //             bitVector.erase(bitVector.begin(), bitVector.begin()+63);
        //         }
        //     }

        // }
        else if(_rule_id==SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID && _c==0)
        {
            if(ack_type == SCHCAckMechanism::ACK_END_WIN || ack_type == SCHCAckMechanism::ACK_END_SES)
            {    
                // * Se ha recibido un SCHC ACK (con errores)
                SPDLOG_DEBUG("Receiving a SCHC ACK with errors");
                int compress_bitmap_len = (len-1)*8 + 5;    // en bits
                char compress_bitmap[compress_bitmap_len];

                int k = 0;
                for(int i=4; i>=0; i--)
                {
                    compress_bitmap[k] = (msg[0] >> i) & 0x01;
                    k++;
                }

                for(int i=1; i<len; i++)
                {
                    for(int j=7; j>=0; j--)
                    {
                        compress_bitmap[k] = (msg[i] >> j) & 0x01;
                        k++;
                    }
                }

                if(compress_bitmap_len >= 63)
                {
                    for(int i=0; i<63; i++)
                    {
                        (*bitmap_array)[_w][i] = compress_bitmap[i];
                    }
                }
                else
                {
                    for(int i=0; i<compress_bitmap_len; i++)
                    {
                        (*bitmap_array)[_w][i] = compress_bitmap[i];
                    }

                    for(int i=compress_bitmap_len; i<63; i++)
                    {
                        (*bitmap_array)[_w][i] = 1;
                    }
                }
            }
            else if(ack_type == SCHCAckMechanism::ACK_COMPOUND)
            {
                // * Se ha recibido un SCHC Compound ACK (con errores)
                SPDLOG_DEBUG("Receiving a SCHC Compound ACK with errors");

                int n_total_bits    = len*8;                        // en bits
                int n_win           = ceil((n_total_bits - 1)/65);  // window_size + M = 65. Se resta un bit a len debido al bit C.
                //int n_padding_bits  = n_total_bits - 1 - n_win*65;
                bool first_win      = true;

                std::vector<uint8_t> bitVector;
                
                /* traspasa el mensaje de formato char a vector*/
                for (int i = 0; i < len; ++i)
                {
                    for (int j = 7; j >= 0; --j)
                    {
                        bitVector.push_back((msg[i] >> j) & 1);
                    }
                }

                for(int i=0; i<n_win; i++)
                {
                    if(first_win)
                    {
                        _windows_with_error.push_back(_w);      // almacena en el vector el numero de la primera ventana con error en el SCHC Compound ACK
                        
                        bitVector.erase(bitVector.begin(), bitVector.begin()+3); // Se elimina del vector la ventana (2 bits) y c (1 bit)
                        std::copy(bitVector.begin(), bitVector.begin() + 63, (*bitmap_array)[_w].begin());
                        bitVector.erase(bitVector.begin(), bitVector.begin()+63);
                        first_win = false;
                    }
                    else
                    {
                        uint8_t win = (bitVector[0] << 1) | bitVector[1];
                        _windows_with_error.push_back(win);
                        bitVector.erase(bitVector.begin(), bitVector.begin()+2);
                        std::copy(bitVector.begin(), bitVector.begin() + 63, (*bitmap_array)[win].begin());
                        bitVector.erase(bitVector.begin(), bitVector.begin()+63);
                    }
                }

            }
        }
          
    }

    return 0;
}

void SCHCNodeMessage::print_msg(SCHCMsgType msgType, const std::vector<uint8_t>& msg, const std::vector<std::vector<uint8_t>>& bitmap_array)
{
    if(msgType == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
    {
        uint8_t schc_header = msg[0];
        uint8_t w_mask      = 0xC0;
        uint8_t fcn_mask    = 0x3F;
        uint8_t w           = (w_mask & schc_header) >> 6;
        uint8_t fcn         = fcn_mask & schc_header;
        int tile_size       = 10;          // hardcoding warning - tile size = 10
        int n_tiles         = (msg.size() - 1)/tile_size; 
        
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "|-----W=%02u, FCN=%02u----->| %02d tiles sent", w, fcn, n_tiles);
        SPDLOG_INFO("{}",buffer);    
    }
    else if(msgType==SCHCMsgType::SCHC_ACK_REQ_MSG)
    {
        uint8_t schc_header = msg[0];
        uint8_t w_mask      = 0xC0;
        uint8_t fcn_mask    = 0x3F;
        uint8_t w           = (w_mask & schc_header) >> 6;
        uint8_t fcn         = fcn_mask & schc_header;

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "|-----W=%02u, FCN=%02u----->|", w, fcn);
        SPDLOG_INFO("{}",buffer);

    }
    else if(msgType==SCHCMsgType::SCHC_SENDER_ABORT_MSG)
    {
        uint8_t schc_header = msg[0];
        uint8_t w_mask      = 0xC0;
        uint8_t fcn_mask    = 0x3F;
        uint8_t w           = (w_mask & schc_header) >> 6;
        uint8_t fcn         = fcn_mask & schc_header;

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "|-----W=%02u, FCN=%02u----->|", w, fcn);
        SPDLOG_INFO("{}",buffer);

    }
    else if(msgType==SCHCMsgType::SCHC_ACK_MSG)
    {
        uint8_t schc_header = msg[0];
        // Mask definition
        uint8_t w_mask = 0xC0;
        uint8_t c_mask = 0x20;
        uint8_t c = (c_mask & schc_header) >> 5;
        uint8_t w = (w_mask & schc_header) >> 6;

        if(c == 1)
        {
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "|<----W=%02u, C=%02u--------| C=1", w, c);
            SPDLOG_INFO("{}",buffer);

        }
        else
        {
            char buffer[128];
            size_t offset = snprintf(buffer, sizeof(buffer), "|<----W=%02u, C=%02u--------| Bitmap: ", w, c);

            for (int i = 0; i < 63; i++)
            {
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%u",bitmap_array[w][i]);
            }

            SPDLOG_INFO("{}",buffer);

        }

    }
    else if(msgType==SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
    {
        uint8_t schc_header = msg[0];
        uint8_t w_mask      = 0xC0;
        uint8_t fcn_mask    = 0x3F;
        uint8_t w           = (w_mask & schc_header) >> 6;
        uint8_t fcn         = fcn_mask;

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "|-----W=%02u, FCN=%02u+RCS->| last tile: %d bits", w, fcn, static_cast<int>((msg.size()-5)*8));
        SPDLOG_INFO("{}",buffer);

    }
    else if(msgType==SCHCMsgType::SCHC_RECEIVER_ABORT_MSG)
    {
        SPDLOG_INFO("|<--SCHC Recv-Abort ---|");
    }
    else if(msgType==SCHCMsgType::SCHC_COMPOUND_ACK)
    {
        uint8_t schc_header = msg[0];
        uint8_t c_mask      = 0x20;
        uint8_t c           = (c_mask & schc_header) >> 5;

        std::string report = fmt::format("|<--- ACK, C={:02} --------|", c);


        for (uint8_t w = 0; w < _windows_with_error.size(); ++w)
        {
            uint8_t win = _windows_with_error[w];

            // VALIDACIÓN DE SEGURIDAD: Evita el Segfault que vimos con ASan
            if (win >= bitmap_array.size()) {
                SPDLOG_ERROR("Acceso ilegal: Ventana {} no existe en bitmap_array", win);
                continue; 
            }

            // 2. Concatenamos la información de la ventana
            report += fmt::format(", W={} - Bitmap:", win);

            // 3. Concatenamos el bitmap bit a bit
            // Usamos reserve para evitar reasignaciones constantes en el string
            report.reserve(report.size() + 63); 

            for (int i = 0; i < 63; ++i)
            {
                // Añadimos el dígito directamente
                report += std::to_string(bitmap_array[win][i]);
            }
        }

        // 4. Imprimimos el string final
        SPDLOG_INFO("{}", report);



    }
    else if(msgType==SCHCMsgType::SCHC_ACK_RESIDUAL_MSG)
    {
        uint8_t schc_header = msg[0];
        // Mask definition
        uint8_t w_mask = 0xC0;
        uint8_t c_mask = 0x20;
        uint8_t c = (c_mask & schc_header) >> 5;
        uint8_t w = (w_mask & schc_header) >> 6;

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "|<----W=%02u, C=%02u --------| Residual ACK, dropped", w, c);
        SPDLOG_INFO("{}",buffer);      
    }

}

void SCHCNodeMessage::printBin(uint8_t val)
{
    SPDLOG_DEBUG("{:08b}", val);
    
}

uint8_t SCHCNodeMessage::get_w()
{
    return _w;
}

std::vector<uint8_t> SCHCNodeMessage::get_w_vector()
{
    return _windows_with_error;
}

uint8_t SCHCNodeMessage::get_c()
{
    return _c;
}

std::vector<uint8_t> SCHCNodeMessage::get_schc_payload()
{
    return _schc_payload;
}