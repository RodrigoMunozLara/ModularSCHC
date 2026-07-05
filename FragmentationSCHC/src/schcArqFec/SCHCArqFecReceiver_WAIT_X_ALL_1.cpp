#include "schcArqFec/SCHCArqFecReceiver_WAIT_X_ALL_1.hpp"
#include <set>
#include <spdlog/fmt/ranges.h>

SCHCArqFecReceiver_WAIT_X_ALL_1::SCHCArqFecReceiver_WAIT_X_ALL_1(SCHCArqFecReceiver& ctx): _ctx(ctx)
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

SCHCArqFecReceiver_WAIT_X_ALL_1::~SCHCArqFecReceiver_WAIT_X_ALL_1()
{
    SPDLOG_DEBUG("Executing SCHCArqFecReceiver_WAIT_X_ALL_1 destructor()");
}

void SCHCArqFecReceiver_WAIT_X_ALL_1::execute(const std::vector<uint8_t>& msg)
{
    SCHCGWMessage           decoder;
    SCHCMsgType             msg_type;       // message type decoded. See message type in SCHC_GW_Macros.hpp
    uint8_t                 w;              // w recibido en el mensaje
    uint8_t                 dtag = 0;       // dtag no es usado en LoRaWAN
    uint8_t                 fcn;            // fcn recibido en el mensaje
    std::vector<uint8_t>    payload;
    int                     payload_len;    // in bits

    SPDLOG_DEBUG("Decoding Message...");
    msg_type = decoder.get_msg_type(_ctx._protoType, _ctx._ruleID, msg);

    if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
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
        SPDLOG_DEBUG("Last Tile received             : {::#x}", _ctx._lastTile);


        //getBitmapArrayFromEncodedMatrixMap();

        /* Checking if there are enough symbols to decode Cmatrix */
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
                SPDLOG_INFO("|--- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - Integrity check: success", w, fcn, _ctx._lastTileSize*8);

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


                /* ToDo: Enqueue schc_packet in Backhaul Core*/
                _ctx._schcCore.handleRxFrame(schc_packet);


                /* State change and timer activation to wait for the last messages 
                that are delayed from the transmitter */
                SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_X_ALL1 --> STATE_RX_END");
                _ctx._nextStateStr = SCHCArqFecReceiverStates::STATE_END;
                _ctx.executeTimer(_ctx._inactivityTimer);
                //_ctx.executeAgain();
                return;
            }
        }
        else
        {
            SPDLOG_INFO("|--- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits - There are not enough symbols", w, fcn, _ctx._lastTileSize*8);

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

            SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_X_ALL1 --> STATE_RX_WAIT_X_MISSING_FRAG");
            _ctx._nextStateStr = SCHCArqFecReceiverStates::STATE_WAIT_X_MISSING_FRAG;
            return;

        }

    }
    else
    {
        SPDLOG_WARN("Only SCHC All-1 message are permitted.");
    }

}

void SCHCArqFecReceiver_WAIT_X_ALL_1::timerExpired()
{
}

void SCHCArqFecReceiver_WAIT_X_ALL_1::release()
{
}

int SCHCArqFecReceiver_WAIT_X_ALL_1::get_tile_ptr(uint8_t window, uint8_t fcn)
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

int SCHCArqFecReceiver_WAIT_X_ALL_1::get_bitmap_ptr(uint8_t fcn)
{
    return (_ctx._windowSize - 1) - fcn;
}

void SCHCArqFecReceiver_WAIT_X_ALL_1::printMatrixHex(const std::vector<std::vector<uint8_t>>& matrix) 
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

void SCHCArqFecReceiver_WAIT_X_ALL_1::decodeCmatrix()
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

void SCHCArqFecReceiver_WAIT_X_ALL_1::storeLastTileinCmatrix(std::vector<uint8_t> lastTile)
{
    // int S = _ctx._rowCount;

    // int cMatrixSize     = _ctx._nsymbols * S * _ctx._mbits;     // bits
    // int tileSize        = _ctx._tileSize * 8;                   // bits
    

    // for(int j=0; j<_ctx._tileSize; j++)
    // {
    //     if((row-1+j) >= S)
    //     {
    //         _ctx._encodedMatrix[row-1+j - S][col] = tile[j]; 
    //         _ctx._encodedMatrixMap[row-1+j - S][col] = 1;
    //     }
    //     else
    //     {
    //         _ctx._encodedMatrix[row-1+j][col-1] = tile[j]; 
    //         _ctx._encodedMatrixMap[row-1+j][col-1] = 1;
    //     }
    // }

    // //printMatrixHex(_ctx._encodedMatrix);
    // //printMatrixHex(_ctx._encodedMatrixMap);

}

std::vector<uint8_t> SCHCArqFecReceiver_WAIT_X_ALL_1::convertDmatrix_to_SCHCPacket()
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

uint32_t SCHCArqFecReceiver_WAIT_X_ALL_1::calculate_crc32(const std::vector<uint8_t>& msg) 
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

bool SCHCArqFecReceiver_WAIT_X_ALL_1::checkEnoughSymbols()
{
    for(const auto& row : _ctx._encodedMatrixMap)
    {
        int sum = std::accumulate(row.begin(), row.end(), 0u);

        if(sum < _ctx._ksymbols )
            return false;
    }

    return true;
}

uint8_t SCHCArqFecReceiver_WAIT_X_ALL_1::get_c_from_bitmap(uint8_t window)
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