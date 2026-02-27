#include "schcArqFec/SCHCArqFecReceiver_WAIT_X_ALL_1.hpp"
#include <schifra/schifra_galois_field.hpp>
#include <schifra/schifra_galois_field_polynomial.hpp>
#include <schifra/schifra_sequential_root_generator_polynomial_creator.hpp>
#include <schifra/schifra_reed_solomon_decoder.hpp>

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
    msg_type = decoder.get_msg_type(ProtocolType::LORAWAN, _ctx._ruleID, msg);

    if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
    {
        decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);

        _ctx._lastTileSize  = decoder.get_schc_payload_len()/8;   // largo del payload SCHC. En bits
        _ctx._last_window   = decoder.get_w();
        _ctx._rcs           = decoder.get_rcs();
        fcn                 = decoder.get_fcn();
        _ctx._lastTile      = decoder.get_schc_payload();           // obtiene el SCHC payload

        _ctx._bitmapArray[_ctx._last_window][_ctx._windowSize-1]  = 1;

        int residualFragmentationBits_size  = ((_ctx._encodedMatrix.size() * _ctx._encodedMatrix[0].size()) % _ctx._tileSize)*8;
        int residualCodingBits_size         = _ctx._lastTileSize * 8 - ((_ctx._encodedMatrix.size() * _ctx._encodedMatrix[0].size()) % _ctx._tileSize)*8;
        SPDLOG_DEBUG("Last Tile size                 : {} bits", _ctx._lastTileSize * 8);
        SPDLOG_DEBUG("Residual fragmentation bits    : {} bits", residualFragmentationBits_size);
        SPDLOG_DEBUG("Residual coding bits + padding : {} bits", residualCodingBits_size);
        SPDLOG_DEBUG("Last Tile received             : {:X}", spdlog::to_hex(_ctx._lastTile));


        /* Storing the Residual fragmentation bits in the C-Matrix */
        int row = _ctx._rowCount - (residualFragmentationBits_size/8);
        int col = _ctx._nsymbols;
        for(int i=0; i < residualFragmentationBits_size/8; i++)
        {
            _ctx._encodedMatrix[row + i][col-1] = _ctx._lastTile[i];
            _ctx._encodedMatrixMap[row + i][col-1] = 1;
        }

        /* Decode CMatrix */
        decodeCmatrix();

        /* Convert D-matrix in a SCHC packet*/
        std::vector<uint8_t> schc_packet = convertDmatrix_to_SCHCPacket();
        schc_packet.insert(schc_packet.end(), _ctx._lastTile.begin() + residualFragmentationBits_size/8, _ctx._lastTile.end());

        uint32_t calculated_rcs = calculate_crc32(schc_packet);
        SPDLOG_DEBUG("Received RCS  : {}",_ctx._rcs);
        SPDLOG_DEBUG("Calculated RCS: {}",calculated_rcs);


        /* ToDo: Call RCS */
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


            SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_X_ALL1 --> STATE_RX_END");
            _ctx._nextStateStr = SCHCArqFecReceiverStates::STATE_END;
            _ctx.executeAgain();
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

   /* Finite Field Parameters */
   const std::size_t field_descriptor                = SCHCArqFecReceiver::_mbits;
   const std::size_t generator_polynomial_index      = 120;
   const std::size_t generator_polynomial_root_count = SCHCArqFecReceiver::_nsymbols - SCHCArqFecReceiver::_ksymbols; 

   /* Reed Solomon Code Parameters */
   const std::size_t code_length = SCHCArqFecReceiver::_nsymbols;
   const std::size_t fec_length  = SCHCArqFecReceiver::_nsymbols - SCHCArqFecReceiver::_ksymbols; ;
   const std::size_t data_length = code_length - fec_length;

   /* Instantiate Finite Field and Generator Polynomials */
   const schifra::galois::field field(field_descriptor,
                                      schifra::galois::primitive_polynomial_size06,
                                      schifra::galois::primitive_polynomial06);

   schifra::galois::field_polynomial generator_polynomial(field);

   if (
        !schifra::make_sequential_root_generator_polynomial(field,
                                                            generator_polynomial_index,
                                                            generator_polynomial_root_count,
                                                            generator_polynomial)
      )
   {
      std::cout << "Error - Failed to create sequential root generator!" << std::endl;
      return;
   }

   /* Instantiate Encoder and Decoder (Codec) */
   //typedef schifra::reed_solomon::shortened_encoder<code_length,fec_length,data_length> encoder_t;
   typedef schifra::reed_solomon::shortened_decoder<code_length,fec_length,data_length> decoder_t;

   //const encoder_t encoder(field,generator_polynomial);
   const decoder_t decoder(field,generator_polynomial_index);


    for (std::size_t i = 0; i < _ctx._encodedMatrix.size(); ++i) 
    {
        /* Instantiate RS Block For Codec */
        schifra::reed_solomon::block<code_length,fec_length> block;

        // Cargamos los datos usando el operador []
        for (std::size_t j = 0; j < code_length; ++j) {
            block[j] = _ctx._encodedMatrix[i][j];
        }

        schifra::reed_solomon::erasure_locations_t erasure_location_list;
        erasure_location_list.clear();
        for(int k=0; k < code_length; k++)
        {
            if(_ctx._encodedMatrixMap[i][k] == 0)
            {
                erasure_location_list.push_back(k);
            }

        }

        // 5. Intento de decodificación con log de error
        bool res = decoder.decode(block, erasure_location_list);
        
        if (!res) {
            // Si entra aquí, imprimiremos los parámetros para debuguear
            SPDLOG_ERROR("Error in row {}: n={}, fec={}", i, code_length, fec_length);
            SPDLOG_DEBUG("block.error_as_string: {}", block.error_as_string());
            continue;
        }

        for (std::size_t j = 0; j < data_length; ++j) {
            _ctx._dataMatrix[i][j] = block[j];
        }
    }   

    SPDLOG_DEBUG("*** D matrix generated ***");

    //printMatrixHex(_ctx._dataMatrix);


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
