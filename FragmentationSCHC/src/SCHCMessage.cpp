#include "SCHCMessage.hpp"


SCHCMessage::SCHCMessage()
{

}

SCHCMessage::~SCHCMessage()
{
    SPDLOG_DEBUG("Executing SCHCMessage destructor()");
}

std::vector<uint8_t> SCHCMessage::create_regular_fragment(uint8_t ruleID, uint8_t dtag, uint8_t w, uint8_t fcn, const std::vector<uint8_t>& payload)
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

std::vector<uint8_t> SCHCMessage::create_ack_request(uint8_t ruleID, uint8_t dtag, uint8_t w)
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

std::vector<uint8_t> SCHCMessage::create_sender_abort(uint8_t ruleID, uint8_t dtag, uint8_t w)
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

std::vector<uint8_t> SCHCMessage::create_all_1_fragment(uint8_t ruleID, uint8_t dtag, uint8_t w, uint32_t rcs, const std::vector<uint8_t>& payload)
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

std::vector<uint8_t> SCHCMessage::create_schc_ack(uint8_t rule_id, uint8_t dtag, uint8_t w, uint8_t c, std::vector<uint8_t> bitmap_vector, bool must_compress)
{
    std::vector<uint8_t> buffer;
    
    uint8_t w_mask      = 0xC0;
    uint8_t c_mask      = 0x20;

    if(c == 1)
    {
        buffer.resize(1);
        // No hay errores, se agregan 5 bits de padding
        char schc_header;  // * Liberada en SCHC_GW_Ack_on_error::RX_RCV_WIN_recv_fragments (linea 220 y 255) y 
        schc_header  = ((w << 6)& w_mask) | ((c << 5) & c_mask) | 0x00;
        buffer[0] = schc_header;
        return buffer;
    }
    else
    {
        // hay errores, se deben calcular los bits de padding según:
        // https://www.rfc-editor.org/rfc/rfc8724.html#name-schc-ack-format

        if(must_compress)
        {
            int last_zero = 0;
            //std::reverse(bitmap_vector.begin(), bitmap_vector.end());
            std::vector<uint8_t> compress_bitmap_vector;
            compress_bitmap_vector.reserve(63);     // window size = 63 in LoRaWAN

            /* Se obtiene la ubicación del ultimo cero revisando de izquierda a derecha en el bitmap*/
            for(size_t i=0; i<bitmap_vector.size(); i++)
            {
                if(bitmap_vector[i] == 0)
                    last_zero = i;
            }

            for(size_t i=0; i<=last_zero; i++)
            {
                compress_bitmap_vector.push_back(bitmap_vector[i]);
            }

            int n_paddin_bits = 8 - ((compress_bitmap_vector.size() + 3) % 8);
            for(int i=0; i< n_paddin_bits; i++)
            {
                compress_bitmap_vector.push_back(1);
            }

            // construye los bits del SCHC packet (header + bitmap) como un vector
            std::vector<uint8_t> bits;
            bits.reserve(66);   // 63 bitmap + 2 bits (w) + 1 bit (c)

            // bits para w y c. Está compuesto por 2 bits. Cada bit lo almacena en un uint8_t
            bits.push_back((w >> 1) & 0b00000001);
            bits.push_back(w & 0b00000001);
            bits.push_back(c & 0b00000001);

            for(int i=0; i<compress_bitmap_vector.size(); i++)
            {
                bits.push_back(compress_bitmap_vector[i]);
            }


            if(bits.size()%8 == 0)
            {
                
                int len = bits.size()/8;
                buffer.resize(len);
                int k=0;
                for(int i=0; i < len; i++)
                {
                    buffer[i] = ((bits[k] << 7) & 0b10000000) |
                                ((bits[k+1] << 6) & 0b01000000) |
                                ((bits[k+2] << 5) & 0b00100000) |
                                ((bits[k+3] << 4) & 0b00010000) |
                                ((bits[k+4] << 3) & 0b00001000) |
                                ((bits[k+5] << 2) & 0b00000100) |
                                ((bits[k+6] << 1) & 0b00000010) |
                                (bits[k+7] & 0b00000001);
                    k = k + 8;
                }
            }
            else
            {
                SPDLOG_ERROR("The compressed bitmap is not a multiple of an L2 word. Review the compress process");
            }
        }
        else
        {
            int n_paddin_bits = 8 - ((bitmap_vector.size() + 3) % 8);
            for(int i=0; i< n_paddin_bits; i++)
            {
                bitmap_vector.push_back(0);
            }

            // construye los bits del SCHC packet (header + bitmap) como un vector
            std::vector<uint8_t> bits;

            // bits para w y c. Está compuesto por 2 bits. Cada bit lo almacena en un uint8_t
            bits.push_back((w >> 1) & 0b00000001);
            bits.push_back(w & 0b00000001);
            bits.push_back(c & 0b00000001);

            for(int i=0; i<bitmap_vector.size(); i++)
            {
                bits.push_back(bitmap_vector[i]);
            }


            if(bits.size()%8 == 0)
            {
                
                int len = bits.size()/8;
                buffer.resize(len);
                int k=0;
                for(int i=0; i < len; i++)
                {
                    buffer[i] = ((bits[k] << 7) & 0b10000000) |
                                ((bits[k+1] << 6) & 0b01000000) |
                                ((bits[k+2] << 5) & 0b00100000) |
                                ((bits[k+3] << 4) & 0b00010000) |
                                ((bits[k+4] << 3) & 0b00001000) |
                                ((bits[k+5] << 2) & 0b00000100) |
                                ((bits[k+6] << 1) & 0b00000010) |
                                (bits[k+7] & 0b00000001);
                    k = k + 8;
                }
            }
            else
            {
                SPDLOG_ERROR("The compressed bitmap is not a multiple of an L2 word. Review the compress process");
            }
        }
    }

    return buffer;
}

std::vector<uint8_t> SCHCMessage::create_schc_ack_compound(uint8_t rule_id, uint8_t dtag, int last_win, const std::vector<uint8_t> c_vector, const std::vector<std::vector<uint8_t>> bitmap_array, uint8_t win_size)
{
    std::vector<uint8_t> buffer;

    if(c_vector.empty())
    {
        // No hay errores, se agregan 5 bits de padding
        buffer.resize(1);

        uint8_t w_mask  = 0xC0;
        uint8_t c_mask  = 0x20;
        uint8_t c       = 1;

        uint8_t schc_header;  // * Liberada en SCHC_GW_Ack_on_error::RX_RCV_WIN_recv_fragments (linea 220 y 255) y 
        schc_header  = ((last_win << 6)& w_mask) | ((c << 5) & c_mask) | 0x00;
        buffer[0] = schc_header;
    }
    else
    {
        /* Construye los bits del SCHC packet (header + bitmap) como un vector */
        std::vector<uint8_t>    bits;
        std::string             bitmap_str = "";
        bool                    first_win_with_error = true;

        for(int i=0; i < c_vector.size(); i++)
        {
            uint8_t w = c_vector[i];
            
            if(first_win_with_error)    // solo para la primera ventana lleva w, c y el bitmap
            {
                // bits para w y c. Está compuesto por 2 bits. Cada bit lo almacena en un uint8_t
                bits.push_back((w >> 1) & 0b00000001);
                bits.push_back(w & 0b00000001);
                bits.push_back(0);      // c = 0

                bitmap_str = bitmap_str + "W=" + std::to_string(w) + " - Bitmap:";
                for(int i=0; i<win_size; i++)
                {
                    bits.push_back(bitmap_array[w][i]);                             // vector que se transformara en un array de char
                    bitmap_str = bitmap_str + std::to_string(bitmap_array[w][i]);   // string para mostrar en pantalla
                }
                first_win_with_error = false;
            }
            else                        // para el resto de las ventanas solo lleva w y el bitmap
            {
                bits.push_back((w >> 1) & 0b00000001);
                bits.push_back(w & 0b00000001);

                bitmap_str = bitmap_str + ", W=" + std::to_string(w) + " - Bitmap:";
                for(int i=0; i<win_size; i++)
                {
                    bits.push_back(bitmap_array[w][i]);                             // vector que se transformara en un array de char
                    bitmap_str = bitmap_str + std::to_string(bitmap_array[w][i]);   // string para mostrar en pantalla
                }

            }            
        }

        /* Se agregan los bits de padding */
        int n_paddin_bits = 8 - (bits.size()% 8);
        for(int i=0; i< n_paddin_bits; i++)
        {
            bits.push_back(0);
        }


        if(bits.size()%8 == 0)
        {    
            int len = bits.size()/8;
            buffer.resize(len);
            int k=0;
            for(int i=0; i < len; i++)
            {
                buffer[i] = ((bits[k] << 7) & 0b10000000) |
                            ((bits[k+1] << 6) & 0b01000000) |
                            ((bits[k+2] << 5) & 0b00100000) |
                            ((bits[k+3] << 4) & 0b00010000) |
                            ((bits[k+4] << 3) & 0b00001000) |
                            ((bits[k+5] << 2) & 0b00000100) |
                            ((bits[k+6] << 1) & 0b00000010) |
                            (bits[k+7] & 0b00000001);
                k = k + 8;
            }

        }

        _compound_ack_string = bitmap_str;
    }

    return buffer;
}

SCHCMsgType SCHCMessage::get_msg_type(ProtocolType protocol, uint8_t rule_id, const std::vector<uint8_t>& msg)
{
    
    if(protocol==ProtocolType::LORAWAN || protocol==ProtocolType::MYRIOTA)
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
    else if(protocol==ProtocolType::LORAWAN_NS || protocol==ProtocolType::MYRIOTA_NS)
    {
        uint8_t schc_header = msg[0];
        int len = msg.size();
        uint8_t fcn_mask = 0x3F;                // Mask definition
        uint8_t _fcn = fcn_mask & schc_header;
        uint8_t _dtag = 0;                      // In LoRaWAN, dtag is not used

        if(rule_id==static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID) && len==1 && _fcn==0)
            _msg_type = SCHCMsgType::SCHC_ACK_REQ_MSG;
        else if(rule_id==static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID) && len==1 && _fcn==63)
            _msg_type = SCHCMsgType::SCHC_SENDER_ABORT_MSG;
        else if (rule_id==static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID) && len>1 && _fcn==63)
            _msg_type = SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG;
        else if (rule_id==static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID) && len>1)
            _msg_type = SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG;
    }


    return _msg_type;
}

uint8_t SCHCMessage::decodeMsg(ProtocolType protocol, int rule_id, const std::vector<uint8_t>& msg, SCHCAckMechanism ack_type, std::vector<std::vector<uint8_t>>* bitmap_array)
{
    if(protocol==ProtocolType::LORAWAN || protocol==ProtocolType::MYRIOTA)
    {
        int len = msg.size();

        SCHCLoRaWANFragRule         _rule_id;
        if(rule_id == 20)
        {
            _rule_id = SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID;
            uint8_t schc_header = msg[0];
            _c = (schc_header >> 5) & 0x01;
            _w = (schc_header >> 6) & 0x03;

            if(_c==1 && len==2 && msg[1] == 0xFF)
            {
                // TODO: Se ha recibido un SCHC Receiver-Abort.
            }
            else if (_c==1)
            {
                SPDLOG_DEBUG("Decoding a SCHC ACK (without errors)");
                if(ack_type == SCHCAckMechanism::ACK_END_WIN || ack_type == SCHCAckMechanism::ACK_END_SES || ack_type == SCHCAckMechanism::ACK_COMPOUND)
                {
                    for(int i=0; i<63; i++)
                    {
                        (*bitmap_array)[_w][i] = 1;
                    }
                }
                else if(ack_type == SCHCAckMechanism::ARQ_FEC)
                {
                    SPDLOG_DEBUG("In an ARQ-FEC mode, a successful SCHC ACK does not use the bitmap concept");                   
                }

            }
            else if (_c==0)
            {
                if(ack_type == SCHCAckMechanism::ACK_END_WIN || ack_type == SCHCAckMechanism::ACK_END_SES)
                {    
                    SPDLOG_DEBUG("Decoding a SCHC ACK (with errors)");
                    int compress_bitmap_len = (len-1)*8 + 5;    // bitmap length
                    char compress_bitmap[compress_bitmap_len];

                    /* Storing the first five bits of the bitmap */
                    for(int i=4; i>=0; i--)
                    {
                        (*bitmap_array)[_w].push_back((msg[0] >> i) & 0x01);
                    }

                    /* Storing the rest of the bitmaps */
                    for(int i=1; i<len; i++)
                    {
                        for(int j=7; j>=0; j--)
                        {
                            (*bitmap_array)[_w].push_back((msg[i] >> j) & 0x01);
                        }
                    }
                }
                else if(ack_type == SCHCAckMechanism::ACK_COMPOUND)
                {
                    SPDLOG_DEBUG("Decoding a SCHC Compound ACK with errors");

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
        else if (rule_id == 21)
        {
            _rule_id = SCHCLoRaWANFragRule::SCHC_FRAG_DOWNDIR_RULE_ID;

            /* ToDo */


        }
        else
        {
            SPDLOG_ERROR("RuleID not supported");
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

    }
    else if(protocol==ProtocolType::LORAWAN_NS || protocol==ProtocolType::MYRIOTA_NS)
    {
        int len = msg.size();

        SCHCLoRaWANFragRule         _rule_id;
        if(rule_id == 20)
        {
            _rule_id = SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID;

            uint8_t schc_header = msg[0];          
            _w                  = (schc_header >> 6) & 0x03;
            _fcn                = schc_header & 0x3F;
            _dtag               = 0;                            // In LoRaWAN, dtag is not used

            if(len==1 && _fcn==0)
            {
                SPDLOG_DEBUG("Decoding SCHC ACK REQ message. RuleID:{}, W:{}, FCN:{}", rule_id, _w, _fcn);
            }
            else if(len==1 && _fcn==63)
            {   
                SPDLOG_DEBUG("Decoding SCHC Sender-Abort message. RuleID:{}, W:{}, FCN:{}", rule_id, _w, _fcn);    
            }
            else if (len>1 && _fcn==63)
            {
                // Crear el uint32_t a partir de los bytes
                _rcs = (static_cast<uint32_t>(msg[1] << 24)) & 0xFF000000 | (static_cast<uint32_t>(msg[2] << 16)) & 0x00FF0000 |
                    (static_cast<uint32_t>(msg[3] << 8)) & 0x0000FF00  | (static_cast<uint32_t>(msg[4])) & 0x000000FF;

                _schc_payload_len   = (len - 5)*8;            // in bits
                _schc_payload.resize(_schc_payload_len/8);

                std::copy(msg.begin() + 5, msg.end(), _schc_payload.begin());

                SPDLOG_DEBUG("Decoding All-1 SCHC message. RuleID:{}, W:{}, FCN:{}, RCS:{}", rule_id, _w, _fcn, _rcs);      
            }
            else if (len>1)
            {
                _schc_payload_len   = (len - 1)*8;    // in bits
                _schc_payload.resize(_schc_payload_len/8);
                std::copy(msg.begin()+1, msg.end(), _schc_payload.begin());
                SPDLOG_DEBUG("Decoding SCHC Regular message. RuleID:{}, W:{}, FCN:{}", rule_id, _w, _fcn);
            }   


        }
        else if (rule_id == 21)
        {
            _rule_id = SCHCLoRaWANFragRule::SCHC_FRAG_DOWNDIR_RULE_ID;

            /* ToDo */
        }

    }

    return 0;
}

void SCHCMessage::print_msg(SCHCMsgType msgType, const std::vector<uint8_t>& msg, const std::vector<std::vector<uint8_t>>& bitmap_array)
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

}

void SCHCMessage::printBin(uint8_t val)
{
    SPDLOG_DEBUG("{:08b}", val);
    
}

uint8_t SCHCMessage::get_w()
{
    return _w;
}

std::vector<uint8_t> SCHCMessage::get_w_vector()
{
    return _windows_with_error;
}

uint8_t SCHCMessage::get_c()
{
    return _c;
}

std::vector<uint8_t> SCHCMessage::get_schc_payload()
{
    return _schc_payload;
}

int SCHCMessage::get_schc_payload_len()
{
    return _schc_payload_len;
}

uint8_t SCHCMessage::get_fcn()
{
    return _fcn;
}

uint8_t SCHCMessage::get_dtag()
{
    return _dtag;
}

std::string SCHCMessage::get_compound_bitmap_str()
{
    return _compound_ack_string;
}

uint32_t SCHCMessage::get_rcs()
{
    return _rcs;
}

