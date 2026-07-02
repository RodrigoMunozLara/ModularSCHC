#include "schcArqFec/SCHCArqFecSender.hpp"
#include "schcArqFec/SCHCArqFecSender_INIT.hpp"
#include <schcAckOnError/SCHCNodeMessage.hpp>


SCHCArqFecSender_INIT::SCHCArqFecSender_INIT(SCHCArqFecSender& ctx): _ctx(ctx)
{

}

SCHCArqFecSender_INIT::~SCHCArqFecSender_INIT()
{
    SPDLOG_DEBUG("Executing SCHCArqFecSender_INIT destructor()");
}

void SCHCArqFecSender_INIT::execute(const std::vector<uint8_t>& msg)
{

    /* Dynamic SCHC parameters */
    _ctx._currentWindow = 0;
    _ctx._currentFcn = (_ctx._windowSize)-1;
    _ctx._currentBitmap_ptr = 0;
    _ctx._currentTile_ptr = 0;


    /* Calculo del RCS de todo el mensaje. Será usado en el envío de un SCHC All-1 fragment*/
    _ctx._rcs = this->calculate_crc32(msg);

    SPDLOG_DEBUG("RCS before to send: {}", _ctx._rcs);


    /* Creating D-Matrix:
        D-matrix is saved in                _ctx._dataMatrix 
        Residual coding bits are saved in   _ctx._residualBitsContainer 
    */
    generateDataMatrix(msg);
    printMatrixHex(_ctx._dataMatrix);


    /* Creating C-Matrix:
        C-matrix is saved in    _ctx._encodedMatrix 
    */
    generateEncodedMatrix(_ctx._dataMatrix);
    printMatrixHex(_ctx._encodedMatrix);

    /* Creating encoded SCHC packet (e-SCHC packet)
        e-SCHC packet = S parameter (1 byte) + C-matrix + Residual coding bits
    */
    std::vector<uint8_t> encodedSchcPacket = generateencodedSCHCpacket(_ctx._encodedMatrix);

    /* Dividing e-SCHC packet in tiles
        tiles are saved in      _ctx._tilesArray
        last tile is saved in   _ctx._lastTile
    */
    divideInTiles(encodedSchcPacket);


    SPDLOG_DEBUG("*** D matrix created ***");
    SPDLOG_DEBUG("Packet size (L):              {} bits", msg.size() * 8);
    SPDLOG_DEBUG("D matrix size:                {} bits", _ctx._tileSize * _ctx._ksymbols * _ctx._mbits);
    SPDLOG_DEBUG("D matrix row (S):             {}", _ctx._tileSize);
    SPDLOG_DEBUG("D matrix col (k):             {}", _ctx._ksymbols);
    SPDLOG_DEBUG("Residual coding bits:         {} bits", _ctx._residualCodingBitsCount);

    SPDLOG_DEBUG("*** C matrix created ***");
    SPDLOG_DEBUG("C matrix size:                {} bits", _ctx._encodedMatrix.size() * _ctx._encodedMatrix[0].size() * _ctx._mbits);
    SPDLOG_DEBUG("C matrix row number (S):      {}", _ctx._encodedMatrix.size());
    SPDLOG_DEBUG("C matrix col number (n):      {}", _ctx._encodedMatrix[0].size());
    SPDLOG_DEBUG("Residual fragmentation bits:  {} bits", (encodedSchcPacket.size() % _ctx._tileSize)*8);
    


    SPDLOG_DEBUG("*** e-SCHC packet created ***");
    SPDLOG_DEBUG("e-SCHC packet = C-matrix");
    SPDLOG_DEBUG("e-SCHC Packet size:           {} bits", encodedSchcPacket.size() * 8);
    SPDLOG_TRACE("Encoded SCHC packet (HEX): {:X}", spdlog::to_hex(encodedSchcPacket));


    SPDLOG_DEBUG("*** Tiles created ***");
    SPDLOG_DEBUG("Full Tiles number:            {} tiles", _ctx._nFullTiles);
    //SPDLOG_DEBUG("LastTile = Residual fragmentation bits + Residual coding bits + Padding");
    SPDLOG_DEBUG("LastTile size:                {} bits", _ctx._lastTile.size()*8);
    SPDLOG_DEBUG("LastTile: {:X}", spdlog::to_hex(_ctx._lastTile));


    /* memory allocated for pointers of each bitmap. */
    _ctx._bitmapArray.resize(_ctx._nWindows);

    /* memory allocated for the 1s and 0s for each bitmap. */ 
    for(int i = 0 ; i < _ctx._nWindows ; i++ )
    {
        _ctx._bitmapArray[i].resize(_ctx._windowSize);
    }



    /* *********** Sending the first SCHC fragment **********/

    /* Number of tiles that can be sent in a payload */
    int payload_available_in_bytes = _ctx._current_L2_MTU - 2; // MTU = SCHC header (1 byte) + k parameter (1 byte) + SCHC payload
    int payload_available_in_tiles = payload_available_in_bytes/_ctx._tileSize;

    /* Temporary variables */
    int n_tiles_to_send     = 0;    // number of tiles to send
    int n_remaining_tiles   = 0;    // Number of remaining tiles to send (used in session confirmation mode)

    SCHCNodeMessage encoder;        // encoder 

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

        /* Agregar el parametro k al comienzo del SCHC payload. Utiliza 1 byte*/
        schc_payload.insert(schc_payload.begin(), _ctx._ksymbols);

        /* Crea un mensaje SCHC en formato hexadecimal */
        _ctx._first_fragment_msg = encoder.create_regular_fragment(_ctx._ruleID, _ctx._dTag, _ctx._currentWindow, _ctx._currentFcn, schc_payload);

        /* Imprime los mensajes para visualizacion ordenada */
        encoder.print_msg(SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG, _ctx._first_fragment_msg);

        /* Envía el mensaje a la capa 2*/
        _ctx._stack->send_frame(_ctx._ruleID, _ctx._first_fragment_msg);

        
        _ctx._currentTile_ptr    = _ctx._currentTile_ptr + n_tiles_to_send;
        _ctx._currentFcn         = _ctx._currentFcn - n_tiles_to_send;


        SPDLOG_DEBUG("Changing STATE: From STATE_INIT --> STATE_SEND");
        _ctx._nextStateStr = SCHCArqFecSenderStates::STATE_SEND;
        _ctx.executeAgain();
        return;
    }

}

void SCHCArqFecSender_INIT::timerExpired()
{
}

void SCHCArqFecSender_INIT::release()
{
}

void SCHCArqFecSender_INIT::divideInTiles(const std::vector<uint8_t>& msg)
{
/* Creates an array of size _nFullTiles x _tileSize to store the message. 
Each row of the array is a tile of tileSize bytes. It also determines 
the number of SCHC windows.*/
SPDLOG_DEBUG("Starting the process of dividing a SCHC packet into tiles");


    _ctx._nFullTiles = msg.size()/_ctx._tileSize;

    /* Converts D-matrix residual bits into bytes. If the number of residual bits obtained 
    when converting an SCHC packet into a D-matrix is not a multiple of 8, padding is used. */
    std::vector<uint8_t> residualBytes = packBitsWithPadding(_ctx._residualBitsContainer);

    int residualCodingBit_size = residualBytes.size();                              // bytes

    _ctx._lastTileSize = residualCodingBit_size;    // bytes


    // memory allocated for elements of rows.
    _ctx._tilesArray.resize(_ctx._nFullTiles);

    // memory allocated for  elements of each column.  
    for(int i = 0 ; i < _ctx._nFullTiles ; i++ )
    {
        _ctx._tilesArray[i].resize(_ctx._tileSize);
    }

    int k=0;
    for(int i=0; i<_ctx._nFullTiles; i++)
    {
        for(int j=0; j<_ctx._tileSize;j++)
        {
            _ctx._tilesArray.at(i).at(j) = msg[k];
            k++;
        }
    }

    //_ctx._lastTile.resize(_ctx._lastTileSize);


    /* Coping the Residual Encoding Bits in LastTile */
    _ctx._lastTile.insert(_ctx._lastTile.begin(), residualBytes.begin(), residualBytes.end());

    /* Numero de ventanas SCHC */
    if(msg.size()>(_ctx._tileSize * _ctx._windowSize*3))
    {
        _ctx._nWindows = 4;
    }
    else if(msg.size()>(_ctx._tileSize * _ctx._windowSize*2))
    {
        _ctx._nWindows = 3;
    }
    else if (msg.size()>(_ctx._tileSize * _ctx._windowSize))
    {
        _ctx._nWindows = 2;
    }
    else
    {
        _ctx._nWindows = 1;
    }

    SPDLOG_DEBUG("Number of SCHC Windows: {} ", _ctx._nWindows);

    return;
}

uint32_t SCHCArqFecSender_INIT::calculate_crc32(const std::vector<uint8_t>& msg) 
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

void SCHCArqFecSender_INIT::generateDataMatrix(const std::vector<uint8_t>& inputBuffer) 
{
    // Calculate total bits (L)
    // We use long long for the intermediate calculation to avoid overflow before the cast
    int totalBits = static_cast<int>(inputBuffer.size()) * 8;

    // Calculate ksymbols and Residual Count
    // Integer division inherently performs the floor operation
    _ctx._overhead                  = _ctx._appConfig.schc.overhead;
    _ctx._S                         = _ctx._tileSize;
    _ctx._ksymbols                  = static_cast<int>(totalBits / (_ctx._S * _ctx._mbits));
    _ctx._residualCodingBitsCount   = static_cast<int>(totalBits % (_ctx._S * _ctx._mbits));
    _ctx._rsymbols                  = ceil(_ctx._ksymbols * _ctx._overhead);
    _ctx._nsymbols                  = _ctx._ksymbols + _ctx._rsymbols; 

    // 4. Initialize Matrix D with dimensions S x k
    _ctx._dataMatrix = std::vector<std::vector<uint8_t>>(_ctx._S, std::vector<uint8_t>(_ctx._ksymbols));

    // 5. Populate the matrix row by row
    // Only full rows are included in the matrix
    for (int i = 0; i < _ctx._S; ++i) 
    {
        for (int j = 0; j < _ctx._ksymbols; ++j) 
        {
            _ctx._dataMatrix[i][j] = inputBuffer[(i * _ctx._ksymbols) + j];
        }
    }

    // 6. Handle residuals
    // Any byte not included in the rowCount rows is moved to the bit container
    _ctx._residualBitsContainer.reserve(_ctx._residualCodingBitsCount);

    int d_matrix_size = _ctx._S * _ctx._ksymbols;
    for (int i = d_matrix_size; i < inputBuffer.size(); ++i) 
    {
        uint8_t currentByte = inputBuffer[i];
        
        // Extract bits from MSB (Most Significant Bit) to LSB
        for (int bitPos = 7; bitPos >= 0; --bitPos)
        {
            uint8_t bitValue = (currentByte >> bitPos) & 1;
            _ctx._residualBitsContainer.push_back(bitValue);
        }
    }


    return;
}

bool SCHCArqFecSender_INIT::generateEncodedMatrix(const std::vector<std::vector<uint8_t>>& matrixD) 
{
    // Initialize Matrix D with dimensions S x k
    _ctx._encodedMatrix = std::vector<std::vector<uint8_t>>(_ctx._tileSize, std::vector<uint8_t>(_ctx._nsymbols));


    correct_reed_solomon* rs = correct_reed_solomon_create(0x11d, 1, 1, _ctx._rsymbols);
    
    if (!rs) {
        SPDLOG_ERROR("Error initializing libcorrect");
        return false;
    }

    for (size_t i = 0; i < _ctx._tileSize; ++i) 
    {
        // Copiar primero los datos puros (95 bytes) al INICIO de la fila codificada
        std::memcpy(_ctx._encodedMatrix[i].data(), _ctx._dataMatrix[i].data(), _ctx._ksymbols);
        
        // Calcular el puntero apuntando JUSTO AL FINAL de los datos (offset de k symbols)
        uint8_t* fec_ptr = _ctx._encodedMatrix[i].data() + _ctx._ksymbols;

        // Extraemos los datos puros para esta fila (deben medir exactamente data_length / K bytes)
        const uint8_t* msg_in = reinterpret_cast<const uint8_t*>(_ctx._dataMatrix[i].data());

        // LLAMADA A LIBCORRECT:
        // Esta función lee 'data_length' bytes de msg_in, realiza la codificación,
        // escribe los datos originales al principio de row_ptr y añade los bytes de FEC
        // al final de row_ptr de forma contigua, llenando los '_nsymbols' completos.
        correct_reed_solomon_encode(
            rs, 
            msg_in, 
            _ctx._ksymbols, 
            fec_ptr  // <--- ¡Aquí se escribe la paridad del código de borrado!
        );

    }   

    correct_reed_solomon_destroy(rs);

    return true;

}

void SCHCArqFecSender_INIT::printMatrixHex(const std::vector<std::vector<uint8_t>>& matrix) 
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

std::vector<uint8_t> SCHCArqFecSender_INIT::generateencodedSCHCpacket(const std::vector<std::vector<uint8_t>>& matrix) 
{
    if (matrix.empty() || matrix[0].empty()) 
    {
        return {};
    }

    size_t rows = matrix.size();
    size_t columns = matrix[0].size();

    std::vector<uint8_t> res;
    res.reserve(rows * columns);

    for (size_t j = 0; j < columns; ++j) 
    {
        for (size_t i = 0; i < rows; ++i) 
        {
            if (j < matrix[i].size()) 
            {
                res.push_back(matrix[i][j]);
            }
        }
    }

    
    return res;

}

std::vector<uint8_t> SCHCArqFecSender_INIT::extractTiles(uint8_t firstTileID, uint8_t nTiles)
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

std::vector<uint8_t> SCHCArqFecSender_INIT::packBitsWithPadding(const std::vector<uint8_t>& bits) 
{
    /**
     * Converts a vector of bits (0s and 1s) into a vector of bytes.
     * If the bit count is not a multiple of 8, it pads the last byte with 
     * trailing zeros to complete the byte.
     * * @param bits Input vector containing 0 or 1 values.
     * @return A vector of uint8_t containing the packed bytes.
     */

    if (bits.empty()) {
        spdlog::warn("Input bit vector is empty. Returning empty byte vector.");
        return {};
    }

    size_t totalBits = bits.size();
    // Calculate the number of bytes needed (rounding up to the nearest multiple of 8)
    size_t totalBytes = (totalBits + 7) / 8; 

    std::vector<uint8_t> packedBytes;
    packedBytes.reserve(totalBytes);

    for (size_t i = 0; i < totalBytes; ++i) 
    {
        uint8_t currentByte = 0;

        for (size_t bitIdx = 0; bitIdx < 8; ++bitIdx) 
        {
            size_t globalBitIndex = i * 8 + bitIdx;

            if (globalBitIndex < totalBits)
            {
                // If we are within the input range, pack the bit
                if (bits[globalBitIndex] == 1) 
                {
                    currentByte |= (1 << (7 - bitIdx)); // MSB-first
                }
            } 
            else 
            {
                // If we exceed the input range, we implicitly pad with 0s
                // (currentByte is already 0-initialized)
                break; 
            }
        }
        packedBytes.push_back(currentByte);
    }

    // Log the result in HEX format as requested
    //SPDLOG_DEBUG("Residual fragmentation bits:");
    //SPDLOG_DEBUG("Converting {} bits in {} bytes", totalBits, totalBytes);
    //SPDLOG_DEBUG("Resulting vector (HEX): {:X}", spdlog::to_hex(packedBytes));

    return packedBytes;
}

