#include "SCHCAckOnErrorSender_INIT.hpp"
#include "SCHCAckOnErrorSender.hpp"


SCHCAckOnErrorSender_INIT::SCHCAckOnErrorSender_INIT(SCHCAckOnErrorSender& ctx): _ctx(ctx)
{

}

SCHCAckOnErrorSender_INIT::~SCHCAckOnErrorSender_INIT()
{
}

void SCHCAckOnErrorSender_INIT::execute(char *msg, int len)
{
    /* Dynamic SCHC parameters */
    _ctx._currentWindow = 0;
    _ctx._currentFcn = (_ctx._windowSize)-1;
    _ctx._currentBitmap_ptr = 0;
    _ctx._currentTile_ptr = 0;

    divideInTiles(msg, len);


    /* memory allocated for pointers of each bitmap. */
    _ctx._bitmapArray = new uint8_t*[_ctx._nWindows];      // * Liberada en SCHC_Node_Ack_on_error::destroy_machine()

    /* memory allocated for the 1s and 0s for each bitmap. */ 
    for(int i = 0 ; i < _ctx._nWindows ; i++ )
    {
        _ctx._bitmapArray[i] = new uint8_t[_ctx._windowSize]; // * Liberada en SCHC_Node_Ack_on_error::destroy_machine()
    }

    /* Setting all bitmaps in 0*/
    for(int i=0; i<_ctx._nWindows; i++)
    {
        for(int j = 0 ; j < _ctx._windowSize ; j++)
        {
            _ctx._bitmapArray[i][j] = 0;
        }
    }

    _ctx.setState(SCHCAckOnErrorSenderStates::STATE_SEND);
    SPDLOG_INFO("Changing STATE: From STATE_TX_INIT --> STATE_TX_SEND");
}

void SCHCAckOnErrorSender_INIT::release()
{
}

void SCHCAckOnErrorSender_INIT::divideInTiles(char *buffer, int len)
{
/* Creates an array of size _nFullTiles x _tileSize to store the message. 
Each row of the array is a tile of tileSize bytes. It also determines 
the number of SCHC windows.*/
SPDLOG_INFO("Starting the process of dividing a SCHC packet into tiles");

/* RFC9011
 5.6.2. Uplink Fragmentation: From Device to SCHC Gateway

Last tile: It can be carried in a Regular SCHC Fragment, alone 
in an All-1 SCHC Fragment, or with any of these two methods. 
Implementations must ensure that:
*/
    if((len%_ctx._tileSize)==0)
    {
        /* El ultimo tile es del tamaño de _TileSize */
        _ctx._nFullTiles = (len/_ctx._tileSize)-1;
        _ctx._lastTileSize = _ctx._tileSize;  
    }
    else
    {
        /* El ultimo tile es menor _TileSize */
        _ctx._nFullTiles = (len/_ctx._tileSize);
        _ctx._lastTileSize = len%_ctx._tileSize;
    }

    SPDLOG_INFO("Full Tiles number: {} tiles", _ctx._nFullTiles);
    SPDLOG_INFO("Last Tile Size: {} bytes", _ctx._lastTileSize);


    // memory allocated for elements of rows.
    _ctx._tilesArray = new char*[_ctx._nFullTiles];           // Liberado en metodo SCHC_Node_Ack_on_error::TX_END_free_resources()

    // memory allocated for  elements of each column.  
    for(int i = 0 ; i < _ctx._nFullTiles ; i++ )
    {
        _ctx._tilesArray[i] = new char[_ctx._tileSize];       // Liberado en metodo SCHC_Node_Ack_on_error::TX_END_free_resources()
    }

    int k=0;
    for(int i=0; i<_ctx._nFullTiles; i++)
    {
        for(int j=0; j<_ctx._tileSize;j++)
        {
            _ctx._tilesArray[i][j] = buffer[k];
            k++;
        }
    }

    _ctx._lastTile = new char[_ctx._lastTileSize];            // Liberado en metodo SCHC_Node_Ack_on_error::TX_END_free_resources()
    for(int i=0; i<_ctx._lastTileSize; i++)
    {
        _ctx._lastTile[i] = buffer[k];
        k++;
    }

    /* Numero de ventanas SCHC */
    if(len>(_ctx._tileSize * _ctx._windowSize*3))
    {
        _ctx._nWindows = 4;
    }
    else if(len>(_ctx._tileSize * _ctx._windowSize*2))
    {
        _ctx._nWindows = 3;
    }
    else if (len>(_ctx._tileSize * _ctx._windowSize))
    {
        _ctx._nWindows = 2;
    }
    else
    {
        _ctx._nWindows = 1;
    }

    SPDLOG_INFO("Number of SCHC Windows: {} ", _ctx._nWindows);


    //this->printTileArray();

    /* Calculo del RCS de todo el mensaje. Será usado en el envío de un SCHC All-1 fragment*/
    _ctx._rcs = this->calculate_crc32(buffer, len);

    SPDLOG_INFO("RCS before to send: {}", _ctx._rcs);

    return;
}

uint32_t SCHCAckOnErrorSender_INIT::calculate_crc32(const char *data, size_t length) 
{
    // CRC32 polynomial (mirrored)
    const uint32_t polynomial = 0xEDB88320;
    uint32_t crc = 0xFFFFFFFF;

    // Process each byte in the buffer
    for (size_t i = 0; i < length; i++) {
        crc ^= static_cast<uint8_t>(data[i]); // Ensure that the data is treated as uint8_t.

        // Process the 8 bits of the byte
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFF;
}
