#include "schcArqFec/SCHCArqFecReceiver.hpp"

SCHCArqFecReceiver_RCV_WINDOW::SCHCArqFecReceiver_RCV_WINDOW(SCHCArqFecReceiver& ctx): _ctx(ctx)
{

}

SCHCArqFecReceiver_RCV_WINDOW::~SCHCArqFecReceiver_RCV_WINDOW()
{
    SPDLOG_DEBUG("Executing SCHCArqFecReceiver_RCV_WINDOW destructor()");
}

void SCHCArqFecReceiver_RCV_WINDOW::execute(const std::vector<uint8_t>& msg)
{

    SCHCGWMessage           decoder;
    SCHCMsgType             msg_type;       // message type decoded. See message type in SCHC_GW_Macros.hpp
    uint8_t                 w;              // w recibido en el mensaje
    uint8_t                 dtag = 0;       // dtag no es usado en LoRaWAN
    uint8_t                 fcn;            // fcn recibido en el mensaje
    std::vector<uint8_t>    payload;
    int                     payload_len;    // in bits

    msg_type = decoder.get_msg_type(_ctx._protoType, _ctx._ruleID, msg);


    if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
    {
        /* Codigo para poder eliminar mensajes de entrada */
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 100);
        int random_number = dist(gen);
        SPDLOG_INFO("\033[31mRandom number: {}\033[0m", random_number);
        SPDLOG_INFO("\033[31mProbability:   {}\033[0m",_ctx._appConfig.schc.error_prob*100);
        if(random_number < _ctx._appConfig.schc.error_prob*100)
        // if(_ctx._counter == 1)
        { 
                SPDLOG_INFO("\033[31mMessage discarded due to error probability\033[0m");   
                return;
        }
        // if(_ctx._counter == 2 || _ctx._counter == 3 || _ctx._counter == 4  || _ctx._counter == 5 )
        // {
        //         SPDLOG_INFO("\033[31mMessage discarded due to error probability\033[0m");   
        //         _ctx._counter++;
        //         return;
        // }
        // else
        // {
        //     _ctx._counter++;
        // }
        

        /* Decoding el SCHC fragment */
        decoder.decode_message(_ctx._protoType, _ctx._ruleID, msg);
        payload_len     = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
        fcn             = decoder.get_fcn();
        w               = decoder.get_w();

        if(w > _ctx._last_window)
            _ctx._last_window    = w;    // aseguro que el ultimo fragmento recibido va a marcar cual es la ultima ventana recibida

        /* Creacion de buffer para almacenar el schc payload del SCHC fragment */
        std::vector<uint8_t> payload = decoder.get_schc_payload();

        /* Extrae el primer byte del payload que corresponde al parametro k*/
        _ctx._ksymbols = static_cast<int>(payload.front());
        payload.erase(payload.begin());
        SPDLOG_DEBUG("k parameter received: {}", _ctx._ksymbols);

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

            /* Storing tiles in C-matrix */
            uint8_t tmp_window  = static_cast<uint8_t>((tile_ptr+i) / _ctx._windowSize);
            uint8_t tmp_fcn     = static_cast<uint8_t>(( (tmp_window + 1) * _ctx._windowSize - 1) - (tile_ptr+i));

            storeTileinCmatrix(_ctx._tilesArray[tile_ptr + i], tmp_window, tmp_fcn);         
        }

        //printMatrixHex(_ctx._encodedMatrix);
        //printMatrixHex(_ctx._encodedMatrixMap);

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



        SPDLOG_DEBUG("Check Enough Symbols: {}", checkEnoughSymbols());
        if(checkEnoughSymbols())
        {
            /* Enviando ACK para confirmar el parametro S*/
            SPDLOG_DEBUG("Sending SCHC ACK");
            SCHCGWMessage    encoder;
            uint8_t c                   = 1;
            uint8_t w                   = 1;
            std::vector<uint8_t> buffer = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c);

            _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --|", w, c);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


            SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_X_ALL1");
            _ctx._nextStateStr = SCHCArqFecReceiverStates::STATE_WAIT_X_ALL1;
            return;
        }
    }
    else if (msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
    {
        decoder.decode_message(_ctx._protoType, _ctx._ruleID, msg);

        _ctx._lastTileSize  = decoder.get_schc_payload_len()/8;   // largo del payload SCHC. En bits
        _ctx._last_window   = decoder.get_w();
        _ctx._rcs           = decoder.get_rcs();
        fcn                 = decoder.get_fcn();
        _ctx._lastTile      = decoder.get_schc_payload();           // obtiene el SCHC payload

        _ctx._bitmapArray[_ctx._last_window][_ctx._windowSize-1]  = 1;

        int residualCodingBits_size         = _ctx._lastTileSize * 8;
        SPDLOG_DEBUG("Last Tile size                 : {} bits", _ctx._lastTileSize * 8);
        SPDLOG_DEBUG("Residual coding bits + padding : {} bits", residualCodingBits_size);
        SPDLOG_DEBUG("Last Tile received             : {:X}", spdlog::to_hex(_ctx._lastTile));


        if(checkEnoughSymbols())
        {
            /* Decode CMatrix */
            decodeCmatrix();

            /* Convert D-matrix in a SCHC packet*/
            std::vector<uint8_t> schc_packet = convertDmatrix_to_SCHCPacket();
            schc_packet.insert(schc_packet.end(), _ctx._lastTile.begin(), _ctx._lastTile.end());

            uint32_t calculated_rcs = calculate_crc32(schc_packet);
            SPDLOG_DEBUG("Received RCS  : {}",_ctx._rcs);
            SPDLOG_DEBUG("Calculated RCS: {}",calculated_rcs);


            if(_ctx._rcs == calculated_rcs)  // * Integrity check: success
            {
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|--- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: success", _ctx._last_window, fcn, _ctx._lastTileSize*8);

                            /* Enviando ACK para confirmar la sesion */
                SPDLOG_DEBUG("Sending SCHC ACK");
                SCHCGWMessage    encoder;
                uint8_t c                   = 1;
                uint8_t w                   = 3;
                std::vector<uint8_t> buffer = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c);

                _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
                SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --|", w, c);
                //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");

                /* Enqueue schc_packet in Backhaul Core*/
                //SPDLOG_DEBUG("Sending IPv6 packet to IPv6 network");
                //_ctx._schcCore.handleRxFrame(schc_packet);

                /* State change and timer activation to wait for the last messages 
                that are delayed from the transmitter */
                SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_END");
                _ctx._nextStateStr = SCHCArqFecReceiverStates::STATE_END;
                _ctx.executeTimer(_ctx._inactivityTimer);
                //_ctx.executeAgain();
                return;
            }
        }
        else
        {
            SPDLOG_INFO("|--- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - There are not enough symbols", _ctx._last_window, fcn, _ctx._lastTileSize*8);

            SPDLOG_DEBUG("Sending SCHC Compound ACK");

            /* Revisa si alguna ventana tiene tiles perdidos */
            std::vector<uint8_t> windows_with_error;
            for(int i=0; i < _ctx._last_window; i++)
            {
                int c = get_c_from_bitmap(i); // Bien usado
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

            SPDLOG_DEBUG("Changing STATE: From STATE_RX_RCV_WINDOW --> STATE_RX_WAIT_X_MISSING_FRAG");
            _ctx._nextStateStr = SCHCArqFecReceiverStates::STATE_WAIT_X_MISSING_FRAG;
            return;
        }

    }
    
    else
    {
        SPDLOG_WARN("Only regular SCHC fragments are permitted.");
    }
 
}

void SCHCArqFecReceiver_RCV_WINDOW::timerExpired()
{
}

void SCHCArqFecReceiver_RCV_WINDOW::release()
{
}

int SCHCArqFecReceiver_RCV_WINDOW::get_tile_ptr(uint8_t window, uint8_t fcn)
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

int SCHCArqFecReceiver_RCV_WINDOW::get_bitmap_ptr(uint8_t fcn)
{
    return (_ctx._windowSize - 1) - fcn;
}

void SCHCArqFecReceiver_RCV_WINDOW::storeTileinCmatrix(std::vector<uint8_t> tile, int w, int fcn)
{

    int col = _ctx._windowSize * (w+1) - fcn - 1;  

    for(int j=0; j<_ctx._tileSize; j++)
    {
        _ctx._encodedMatrix[j][col] = tile[j];
        _ctx._encodedMatrixMap[j][col] = 1;
    }

    //printMatrixHex(_ctx._encodedMatrix);
    //printMatrixHex(_ctx._encodedMatrixMap);

}

void SCHCArqFecReceiver_RCV_WINDOW::printMatrixHex(const std::vector<std::vector<uint8_t>>& matrix) 
{
    std::cout << "--- Matrix Hex Dump ---" << std::endl;
    
    for (size_t i = 0; i < matrix.size(); ++i) {
        std::cout << "Fila [" << std::setw(2) << std::setfill('0') << i+1 << "]: ";
        
        for (size_t j = 0; j < matrix[i].size(); ++j) {
            // Imprime cada byte como Hexadecimal de 2 dígitos
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(matrix[i][j]) << " ";
        }
        
        std::cout << std::dec << std::endl; // Volver a decimal para el resto del programa
    }
    std::cout << "-----------------------" << std::endl;
}

bool SCHCArqFecReceiver_RCV_WINDOW::checkEnoughSymbols()
{
    for(const auto& row : _ctx._encodedMatrixMap)
    {
        int sum = std::accumulate(row.begin(), row.end(), 0u);

        if(sum < _ctx._ksymbols )
            return false;
    }

    return true;
}

void SCHCArqFecReceiver_RCV_WINDOW::decodeCmatrix()
{
    correct_reed_solomon* rs = correct_reed_solomon_create(0x11d, 1, 1, _ctx._rsymbols);
    
    if (!rs) {
        SPDLOG_ERROR("Error initializing libcorrect for decoding");
        return;
    }

    for (int i = 0; i < _ctx._tileSize; i++) 
    {
        std::vector<uint8_t> erasure_locations;
        erasure_locations.reserve(_ctx._nsymbols);

        for (size_t col = 0; col < _ctx._nsymbols; ++col) 
        {
            // Cambiado [0] por [i] para leer el mapa real de esta fila
            if (_ctx._encodedMatrixMap[i][col] == 0) { 
                erasure_locations.push_back(static_cast<uint8_t>(col));
            }
        }

        // Validación defensiva por cada fila antes de intentar decodificar
        if (erasure_locations.size() > static_cast<size_t>(_ctx._rsymbols)) 
        {
            SPDLOG_ERROR("Unable to decode row {}: {} columns lost, limit is {}", 
                        i, erasure_locations.size(), _ctx._rsymbols);
            break; // Falla la ventana completa, activar ARQ
        }

        //SPDLOG_DEBUG("Row {} -> erasure_locations: [{}]", i, fmt::join(erasure_locations, ", "));


        uint8_t erasure_locations_buffer[erasure_locations.size()];
        std::memcpy(erasure_locations_buffer, erasure_locations.data(), erasure_locations.size() * sizeof(uint8_t));
        uint8_t dataMatrix[_ctx._ksymbols];


        ssize_t result = correct_reed_solomon_decode_with_erasures(rs, _ctx._encodedMatrix[i].data(), _ctx._nsymbols, erasure_locations_buffer, erasure_locations.size(), dataMatrix);

        if (result < 0) {
            SPDLOG_ERROR("Total failure. More than {} symbols were lost in row {}", _ctx._rsymbols, i );
            break;
        } else {
            SPDLOG_DEBUG("Row {} reconstructed using deletion codes", i);

            _ctx._dataMatrix[i].assign(dataMatrix, dataMatrix + _ctx._ksymbols);

        }
    }
    // Liberar recursos de la librería
    correct_reed_solomon_destroy(rs);

    return;
}

std::vector<uint8_t> SCHCArqFecReceiver_RCV_WINDOW::convertDmatrix_to_SCHCPacket()
{
    std::vector<uint8_t> flatVector;

    // 1. Calcular el tamaño total
    size_t totalSize = 0;
    for (const auto& row : _ctx._dataMatrix) {
        totalSize += row.size();
    }

    // 2. Reservar memoria (Crucial para el rendimiento)
    flatVector.reserve(totalSize);

    // 3. Copiar fila por fila
    for (const auto& row : _ctx._dataMatrix) {
        flatVector.insert(flatVector.end(), row.begin(), row.end());
    }    

    return flatVector;
}

uint32_t SCHCArqFecReceiver_RCV_WINDOW::calculate_crc32(const std::vector<uint8_t>& msg) 
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

uint8_t SCHCArqFecReceiver_RCV_WINDOW::get_c_from_bitmap(uint8_t window)
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