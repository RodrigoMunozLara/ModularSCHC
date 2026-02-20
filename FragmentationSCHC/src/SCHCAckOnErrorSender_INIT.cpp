#include "SCHCAckOnErrorSender_INIT.hpp"
#include "SCHCAckOnErrorSender.hpp"


SCHCAckOnErrorSender_INIT::SCHCAckOnErrorSender_INIT(SCHCAckOnErrorSender& ctx): _ctx(ctx)
{

}

SCHCAckOnErrorSender_INIT::~SCHCAckOnErrorSender_INIT()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorSender_INIT destructor()");
}

void SCHCAckOnErrorSender_INIT::execute(const std::vector<uint8_t>& msg)
{

    if(_ctx._appConfig.schc.schc_l2_protocol.compare("lorawan") == 0)
    {
        /*
             8.4.3.1. Sender Behavior

            At the beginning of the fragmentation of a new SCHC Packet:
            the fragment sender MUST select a RuleID and DTag value pair 
            for this SCHC Packet. A Rule MUST NOT be selected if the values 
            of M and WINDOW_SIZE for that Rule are such that the SCHC Packet 
            cannot be fragmented in (2^M) * WINDOW_SIZE tiles or less.
            the fragment sender MUST initialize the Attempts counter to 0 for 
            that RuleID and DTag value pair
        */

        if(msg.size() > (pow(2,_ctx._m)*_ctx._windowSize)*_ctx._tileSize)
        {
            SPDLOG_WARN("The message is larger than {} tiles ", (int)pow(2,_ctx._m)*_ctx._windowSize);
            return;
        }
        else
        {
            /* Dynamic SCHC parameters */
            _ctx._currentWindow = 0;
            _ctx._currentFcn = (_ctx._windowSize)-1;
            _ctx._currentBitmap_ptr = 0;
            _ctx._currentTile_ptr = 0;

            divideInTiles(msg);


            /* memory allocated for pointers of each bitmap. */
            _ctx._bitmapArray.resize(_ctx._nWindows);

            /* memory allocated for the 1s and 0s for each bitmap. */ 
            for(int i = 0 ; i < _ctx._nWindows ; i++ )
            {
                _ctx._bitmapArray[i].resize(_ctx._windowSize);
            }

            SPDLOG_DEBUG("Changing STATE: From STATE_TX_INIT --> STATE_TX_SEND");
            _ctx._nextStateStr = SCHCAckOnErrorSenderStates::STATE_SEND;
            _ctx.executeAgain();
        }
    }    





}

void SCHCAckOnErrorSender_INIT::timerExpired()
{
}

void SCHCAckOnErrorSender_INIT::release()
{
}

void SCHCAckOnErrorSender_INIT::divideInTiles(const std::vector<uint8_t>& msg)
{
/* Creates an array of size _nFullTiles x _tileSize to store the message. 
Each row of the array is a tile of tileSize bytes. It also determines 
the number of SCHC windows.*/
SPDLOG_DEBUG("Starting the process of dividing a SCHC packet into tiles");

/* RFC9011
 5.6.2. Uplink Fragmentation: From Device to SCHC Gateway

Last tile: It can be carried in a Regular SCHC Fragment, alone 
in an All-1 SCHC Fragment, or with any of these two methods. 
Implementations must ensure that:
*/
    if((msg.size() % _ctx._tileSize) == 0)
    {
        /* El ultimo tile es del tamaño de _TileSize */
        _ctx._nFullTiles = (msg.size()/_ctx._tileSize)-1;
        _ctx._lastTileSize = _ctx._tileSize;  
    }
    else
    {
        /* El ultimo tile es menor _TileSize */
        _ctx._nFullTiles = (msg.size()/_ctx._tileSize);
        _ctx._lastTileSize = msg.size()%_ctx._tileSize;
    }

    SPDLOG_DEBUG("Full Tiles number: {} tiles", _ctx._nFullTiles);
    SPDLOG_DEBUG("Last Tile Size: {} bytes", _ctx._lastTileSize);


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

    _ctx._lastTile.resize(_ctx._lastTileSize);

    for(int i=0; i<_ctx._lastTileSize; i++)
    {
        _ctx._lastTile.at(i) = msg[k];
        k++;
    }

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


    //this->printTileArray();

    /* Calculo del RCS de todo el mensaje. Será usado en el envío de un SCHC All-1 fragment*/
    _ctx._rcs = this->calculate_crc32(msg);

    SPDLOG_DEBUG("RCS before to send: {}", _ctx._rcs);

    return;
}

uint32_t SCHCAckOnErrorSender_INIT::calculate_crc32(const std::vector<uint8_t>& msg) 
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
