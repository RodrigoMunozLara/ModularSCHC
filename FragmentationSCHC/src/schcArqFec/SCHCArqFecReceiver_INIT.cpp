#include "schcArqFec/SCHCArqFecReceiver_INIT.hpp"


SCHCArqFecReceiver_INIT::SCHCArqFecReceiver_INIT(SCHCArqFecReceiver& ctx): _ctx(ctx)
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
    _ctx._bitmapArray.resize(_ctx._nMaxWindows);

    /* memory allocated for the 1s and 0s for each bitmap. */ 
    for(int i = 0 ; i < _ctx._nMaxWindows ; i++ )
    {
        _ctx._bitmapArray[i].resize(_ctx._windowSize);
    }

}

SCHCArqFecReceiver_INIT::~SCHCArqFecReceiver_INIT()
{
    SPDLOG_DEBUG("Executing SCHCArqFecReceiver_INIT destructor()");
}

void SCHCArqFecReceiver_INIT::execute(const std::vector<uint8_t>& msg)
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
        // if(_ctx._counter == 2 || _ctx._counter == 4 ||  _ctx._counter == 9)
        // {
        //         //SPDLOG_WARN("\033[31mMessage discarded due to error probability\033[0m");   
        //         SPDLOG_INFO("\033[31mMessage discarded due to error probability\033[0m");   
        //         _ctx._counter++;
        //         return;
        // }
        // _ctx._counter++;

        /* Decoding el SCHC fragment */
        decoder.decode_message(ProtocolType::LORAWAN, _ctx._ruleID, msg);
        fcn = decoder.get_fcn();
        
        if(fcn == (_ctx._windowSize - 1))
        {
            w = decoder.get_w();

            if(w > _ctx._last_window)
                _ctx._last_window    = w;    // aseguro que el ultimo fragmento recibido va a marcar cual es la ultima ventana recibida


            /* Se recibe el primer SCHC fragment con el parametro S en el primer byte*/
            payload_len     = decoder.get_schc_payload_len();   // largo del payload SCHC. En bits
            
            /* Creacion de buffer para almacenar el schc payload del SCHC fragment */
            payload = decoder.get_schc_payload();

            /* Extrae el primer byte del payload que corresponde al parametro S*/
            _ctx._rowCount = static_cast<int>(payload.front());
            payload.erase(payload.begin());
            SPDLOG_DEBUG("S parameter received: {}", _ctx._rowCount);

            /* Inicializar memoria para matrices FEC*/
            initializeMatrices(_ctx._rowCount, _ctx._ksymbols, _ctx._nsymbols);

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


            /* Enviando ACK para confirmar el parametro S*/
            SPDLOG_DEBUG("Sending SCHC ACK");
            SCHCGWMessage    encoder;
            uint8_t c                           = 1;    // Bien usado  
            std::vector<uint8_t> buffer         = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c);

            _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t] %v");
            SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --|", w, c);
            //spdlog::set_pattern("[%H:%M:%S.%e][%^%L%$][%t][%-8!s][%-8!!] %v");


            SPDLOG_DEBUG("Changing STATE: From STATE_RX_INIT --> STATE_RX_RCV_WINDOW");
            _ctx._nextStateStr = SCHCArqFecReceiverStates::STATE_RCV_WINDOW;
            return;

        }
        else
        {
            /* ToDo: ERROR, no se puede recibir un fragmento diferente del primero */
            SPDLOG_ERROR("A fragment cannot be received with a tile different from WindowSize - 1.");
        }
    }
    else
    {
        SPDLOG_WARN("Only regular SCHC fragments are permitted.");
    }

}

void SCHCArqFecReceiver_INIT::timerExpired()
{
}

void SCHCArqFecReceiver_INIT::release()
{
}

int SCHCArqFecReceiver_INIT::get_tile_ptr(uint8_t window, uint8_t fcn)
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

int SCHCArqFecReceiver_INIT::get_bitmap_ptr(uint8_t fcn)
{
    return (_ctx._windowSize - 1) - fcn;
}

void SCHCArqFecReceiver_INIT::initializeMatrices(size_t S, size_t k, size_t n) 
{
    // 1. Allocate rows for Data Matrix (S rows)
    _ctx._dataMatrix.resize(S);
    for (size_t i = 0; i < S; ++i) {
        _ctx._dataMatrix[i].resize(k); // Allocate k columns per row
    }

    // 2. Allocate rows for Encoded Matrix (S rows)
    _ctx._encodedMatrix.resize(S);
    for (size_t i = 0; i < S; ++i) {
        _ctx._encodedMatrix[i].resize(n); // Allocate n columns per row
    }

    // 3. Allocate rows for Encoded Matrix Map (S rows)
    _ctx._encodedMatrixMap.assign(S, std::vector<uint8_t>(n, 0));

    spdlog::info("Matrices initialized: Data({}x{}), Encoded({}x{}), Map({}x{})", 
                 S, k, S, n, S, n);
}

void SCHCArqFecReceiver_INIT::storeTileinCmatrix(std::vector<uint8_t> tile, int w, int fcn)
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

void SCHCArqFecReceiver_INIT::printMatrixHex(const std::vector<std::vector<uint8_t>>& matrix) 
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
