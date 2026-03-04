#include "schcArqFec/SCHCArqFecReceiver.hpp"
#include <schifra/schifra_galois_field.hpp>
#include <schifra/schifra_galois_field_polynomial.hpp>
#include <schifra/schifra_sequential_root_generator_polynomial_creator.hpp>
#include <schifra/schifra_reed_solomon_decoder.hpp>

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

    msg_type = decoder.get_msg_type(ProtocolType::LORAWAN, _ctx._ruleID, msg);


    if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
    {
        /* Codigo para poder eliminar mensajes de entrada */
        // random_device rd;
        // mt19937 gen(rd());
        // uniform_int_distribution<int> dist(0, 100);
        // int random_number = dist(gen);
        //if(random_number < _error_prob)
        //if(_ctx._counter == 2 || _ctx._counter == 3 || _ctx._counter == 4 || _ctx._counter == 6 || _ctx._counter == 7 )
        // if(_ctx._counter == 2 || _ctx._counter == 3 || _ctx._counter == 7 )
        // {
        //         //SPDLOG_WARN("\033[31mMessage discarded due to error probability\033[0m");   
        //         SPDLOG_INFO("\033[31mMessage discarded due to error probability\033[0m");   
        //         _ctx._counter++;
        //         return;
        // }
        // _ctx._counter++;

        /* Decoding el SCHC fragment */
        decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
        payload_len     = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
        fcn             = decoder.get_fcn();
        w               = decoder.get_w();

        if(fcn == _ctx._windowSize - 1)
        {
            SPDLOG_DEBUG("Discarding message. Previously received message");
            return;
        }

        if(w > _ctx._last_window)
            _ctx._last_window    = w;    // aseguro que el ultimo fragmento recibido va a marcar cual es la ultima ventana recibida

        /* Creacion de buffer para almacenar el schc payload del SCHC fragment */
        std::vector<uint8_t> payload = decoder.get_schc_payload();

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

        if(checkEnoughSymbols())
        {
            /* Decode CMatrix */
            decodeCmatrix();

            /* Convert D-matrix in a SCHC packet*/
            std::vector<uint8_t> schc_packet = convertDmatrix_to_SCHCPacket();
            schc_packet.insert(schc_packet.end(), _ctx._lastTile.begin() + residualFragmentationBits_size/8, _ctx._lastTile.end());

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


                SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_X_ALL1 --> STATE_RX_END");
                _ctx._nextStateStr = SCHCArqFecReceiverStates::STATE_END;
                _ctx.executeAgain();
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

            SPDLOG_DEBUG("Changing STATE: From STATE_RX_WAIT_X_ALL1 --> STATE_RX_WAIT_X_MISSING_FRAG");
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
    int S = _ctx._rowCount;

    /* correlative tile number (ctn). ctn starts in 1 */
    int ctn = _ctx._windowSize * (w+1) - fcn;  
    
    /* row = (ctn-1)*ts + 1 - floor((ctn-1)*ts/S)*S. Row starts in 1 */
    int row = (ctn-1)*_ctx._tileSize + 1 - std::floor((ctn-1)*_ctx._tileSize/S)*S;
    
    /* column = floor((ctn-1)*ts/S) + 1. Column starts in 1 */
    int col = std::floor((ctn-1)*_ctx._tileSize/S) + 1; 

    

    for(int j=0; j<_ctx._tileSize; j++)
    {
        if((row-1+j) >= S)
        {
            _ctx._encodedMatrix[row-1+j - S][col] = tile[j]; 
            _ctx._encodedMatrixMap[row-1+j - S][col] = 1;
        }
        else
        {
            _ctx._encodedMatrix[row-1+j][col-1] = tile[j]; 
            _ctx._encodedMatrixMap[row-1+j][col-1] = 1;
        }
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