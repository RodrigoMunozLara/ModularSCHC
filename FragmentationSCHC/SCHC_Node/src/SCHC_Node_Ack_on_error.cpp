#include "SCHC_Node_Ack_on_error.hpp"

bool    _finish_loop;
bool    _finish_loop_ack;

std::deque<std::tuple<char*, int>>   _queue;

void finish_loop();
void finish_loop_ack();

SCHC_Node_Ack_on_error::SCHC_Node_Ack_on_error()
{
}

SCHC_Node_Ack_on_error::~SCHC_Node_Ack_on_error()
{
}

uint8_t SCHC_Node_Ack_on_error::init(uint8_t ruleID, uint8_t dTag, uint8_t windowSize, uint8_t tileSize, uint8_t n, uint8_t ackMode, SCHC_Node_Stack_L2 *stack_ptr, int retTimer, uint8_t ackReqAttempts)
{
    SPDLOG_TRACE("Entering the function");
    /* Static SCHC parameters */
    _currentState = STATE_TX_INIT;
    _ruleID                 = ruleID;
    _dTag                   = dTag;
    _windowSize             = windowSize;
    _nFullTiles             = 0;                // in tiles
    _lastTileSize           = 0;              // in bytes
    _tileSize               = tileSize;           // in bytes
    _ackMode                = ackMode;
    _retransTimer           = static_cast<unsigned long>(retTimer);       // in millis
    _maxAckReq              = ackReqAttempts;
    _retransTimer_counter   = 0;      // in seconds
    _loop_counter           = 0;
    _all_tiles_sent         = false;
    _last_confirmed_window  = -1;

    /* Static LoRaWAN parameters*/
    _current_L2_MTU         = stack_ptr->getMtu(true);
    _stack                  = stack_ptr;

    _running                = true;
    _retrans_ack_req_flag   = false;
    _send_schc_ack_req_flag = false;

    SPDLOG_TRACE("Leaving the function");

    return 0;
}

uint8_t SCHC_Node_Ack_on_error::start(char *msg, int len)
{
    this->TX_INIT_send_fragments(msg, len);
    return 0;
}

uint8_t SCHC_Node_Ack_on_error::enqueue_message(char* msg, int len)
{
    std::tuple<char*, int> myTuple(msg, len);
    _queue.push_back(myTuple);

    SPDLOG_TRACE("SCHC_Node_Ack_on_error::queue_message - Data successfully queue");


    return 0;
}

uint8_t SCHC_Node_Ack_on_error::execute(char *msg, int len)
{
    SPDLOG_TRACE("Entering the function");
    if(msg!=NULL)
    {
        if(_currentState==STATE_TX_INIT)
        {
            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Calling to TX_INIT_send_fragments()");

            this->TX_INIT_send_fragments(msg, len);

        }
        else if(_currentState==STATE_TX_WAIT_x_ACK)
        {
            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Calling to TX_WAIT_x_ACK_receive_ack()");

            this->TX_WAIT_x_ACK_receive_ack(msg, len);
        }
        else if(_currentState==STATE_TX_SEND)
        {
            SPDLOG_TRACE("In the STATE_TX_SEND state, no messages are expected. Discarding mesg.");
        }     
        else if(_currentState==STATE_TX_RESEND_MISSING_FRAG)
        {
            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Calling to TX_RESEND_MISSING_FRAG_send_fragments()");

            this->TX_RESEND_MISSING_FRAG_send_fragments(msg, len);
        }    
        else
        {

            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Warning!!, there is no assigned method to process the message");

        }
    }
    else
    {
        if(_currentState==STATE_TX_SEND)
        {
            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Calling to TX_SEND_send_fragments()");

            this->TX_SEND_send_fragments();
        }
        else if(_currentState==STATE_TX_END)
        {
            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Calling to TX_END_free_resources()");

            this->TX_END_free_resources();
        }
        else if(_currentState==STATE_TX_RESEND_MISSING_FRAG)
        {
            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Calling to TX_RESEND_MISSING_FRAG_send_fragments()");

            this->TX_RESEND_MISSING_FRAG_send_fragments();
        }
        else if(_currentState==STATE_TX_WAIT_x_ACK)
        {
            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Do nothing!!");

        }
        else
        {

            SPDLOG_TRACE("SCHC_Node_Ack_on_error::execute - Warning!!, there is no assigned method to process the message");
 
        }
    }


    SPDLOG_TRACE("Leaving the function");
    return 0;
}

void SCHC_Node_Ack_on_error::message_reception_loop()
{
    SPDLOG_TRACE("SCHC_Node_Ack_on_error::message_reception_loop - Starting message reception loop.");

    int main_loop_wait_time = 50;     // miliseconds
    _finish_loop_ack        = true;
    //Ticker main_loop_ticker(finish_loop, main_loop_wait_time);
    //Ticker retr_ack_req_ticker(finish_loop_ack, _retransTimer*1000);
    
    
    while (_running)
    {
        _finish_loop = true;

        this->execute();

        if (!_queue.empty())
        {
            SPDLOG_TRACE("SCHC_Node_Ack_on_error::message_reception_loop - Extracting message from the queue.");


            std::tuple<char*, int> myTuple = _queue.front();
            char* buffer    = std::get<0>(myTuple);
            int value       = std::get<1>(myTuple);
            
            _queue.pop_front();
            this->execute(buffer, value);
        }

        /* Delay para la retransmision de un SCHC ACK */
        if(_retrans_ack_req_flag)
        {
            //retr_ack_req_ticker.start();
            while(_finish_loop_ack)
            {
                //retr_ack_req_ticker.update();
            }
            //retr_ack_req_ticker.stop();

            SPDLOG_TRACE("| Retran Timer expired |");

            
            this->send_ack_req();
            if(_retransTimer_counter >= _maxAckReq)
            {
                this->send_sender_abort();
                this->TX_END_free_resources();
            }
        }
        

        /* Delay para el loop principal */
        //main_loop_ticker.start();
        while(_finish_loop)
        {
            //main_loop_ticker.update();
        }
        //main_loop_ticker.stop();
    }


    SPDLOG_TRACE("Leaving the function");
    
}

uint8_t SCHC_Node_Ack_on_error::TX_INIT_send_fragments(char *msg, int len)
{

    SPDLOG_TRACE("Entering the function");

    /* Dynamic SCHC parameters */
    _currentWindow = 0;
    _currentFcn = (_windowSize)-1;
    _currentBitmap_ptr = 0;
    _currentTile_ptr = 0;

    uint8_t res;
    res = this->divideInTiles(msg, len);
    if(res==1)
    {
        SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_INIT_send_fragments - ERROR when splitting the message into tiles");
    }


    /* memory allocated for pointers of each bitmap. */
    _bitmapArray = new uint8_t*[_nWindows];      // * Liberada en SCHC_Node_Ack_on_error::destroy_machine()

    /* memory allocated for the 1s and 0s for each bitmap. */ 
    for(int i = 0 ; i < _nWindows ; i++ )
    {
        _bitmapArray[i] = new uint8_t[_windowSize]; // * Liberada en SCHC_Node_Ack_on_error::destroy_machine()
    }

    /* Setting all bitmaps in 0*/
    for(int i=0; i<_nWindows; i++)
    {
        for(int j = 0 ; j < _windowSize ; j++)
        {
            _bitmapArray[i][j] = 0;
        }
    }


    _currentState = STATE_TX_SEND;

    SPDLOG_INFO("Changing STATE: From STATE_TX_INIT --> STATE_TX_SEND");


    this->message_reception_loop();

    SPDLOG_TRACE("Leaving the function");
    return res;
}

uint8_t SCHC_Node_Ack_on_error::TX_SEND_send_fragments()
{
    SPDLOG_TRACE("Entering the function");

    /* Numero de tiles que se pueden enviar un un payload */
    int payload_available_in_bytes = _current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
    int payload_available_in_tiles = payload_available_in_bytes/_tileSize;

    /* Variables temporales */
    int n_tiles_to_send     = 0;    // numero de tiles a enviar
    int n_remaining_tiles   = 0;    // n de tiles restantes por enviar (usado en el modo de confirmacion por sesion)
   
    SCHC_Node_Message encoder;           // encoder 

    if(_ackMode==ACK_MODE_ACK_END_WIN)
    {
        if(_send_schc_ack_req_flag == true)
        {
            /* ******************* SCHC ACK REQ ********************************* */
            /* Se envía un SCHC ACK REQ para empujar el envio en el downlink
            del SCHC ACK enviado por el SCHC Gateway */
            SCHC_Node_Message encoder_2;
            char* schc_ack_req_msg      = new char[1];      // liberado en linea 308
            int schc_ack_req_msg_len    = encoder_2.create_ack_request(_ruleID, _dTag, _currentWindow, schc_ack_req_msg);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder_2.print_msg(SCHC_ACK_REQ_MSG, schc_ack_req_msg, schc_ack_req_msg_len);

            /* Envía el mensaje a la capa 2*/
            int res = _stack->send_frame(_ruleID, schc_ack_req_msg, schc_ack_req_msg_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_ack_req_msg;

            _currentState           = STATE_TX_WAIT_x_ACK;
            _retrans_ack_req_flag   = true;
            _send_schc_ack_req_flag = false;

            SPDLOG_INFO("Changing STATE: From STATE_TX_SEND --> STATE_TX_WAIT_x_ACK");


            SPDLOG_TRACE("Leaving the function");
            
            return 0;
        }


        /* Determina el numero de tiles restantes de enviar*/
        if(_currentWindow == (_nWindows - 1))
        {
            n_remaining_tiles = _nFullTiles - _currentTile_ptr;
        }     
        else
        {
            n_remaining_tiles = _currentFcn + 1;
        }
            
        if(n_remaining_tiles>payload_available_in_tiles)
        {
            /* Si la cantidad de tiles que faltan por enviar 
            para la ventana i-esima no alcanza en la MTU 
            significa que aun NO finaliza la ventana i-esima
            */

            n_tiles_to_send = payload_available_in_tiles;

            /* buffer que almacena todos los tiles que se van a enviar */
            int payload_len         = n_tiles_to_send * _tileSize;  // tamaño del SCHC payload en bytes
            char* schc_payload      = new char[payload_len];        // liberado en linea 215. buffer para el SCHC payload
            char* schc_message      = new char[payload_len + 1];    // liberado en linea 214 buffer para el SCHC message (header + payload)            
            
            this->extractTiles(_currentTile_ptr, n_tiles_to_send, schc_payload);

            /* Crea un mensaje SCHC en formato hexadecimal */
            int schc_message_len = encoder.create_regular_fragment(_ruleID, _dTag, _currentWindow, _currentFcn, schc_payload, payload_len, schc_message);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHC_REGULAR_FRAGMENT_MSG, schc_message, schc_message_len);

            /* Envía el mensaje a la capa 2*/
            uint8_t res = _stack->send_frame(_ruleID, schc_message, schc_message_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_message;
            delete[] schc_payload;
            
            _currentTile_ptr    = _currentTile_ptr + n_tiles_to_send;
            _currentFcn         = _currentFcn - n_tiles_to_send;
        }
        else
        {
            /* Si la cantidad de tiles que faltan por enviar 
            para la ventana i-esima alcanza en la MTU 
            significa que este es el ultimo envio para la 
            ventana i-esima
            */

            n_tiles_to_send = n_remaining_tiles;

            /* buffer que almacena todos los tiles que se van a enviar */
            int payload_len         = n_tiles_to_send * _tileSize;  // tamaño del SCHC payload en bytes
            char* schc_payload      = new char[payload_len];        // liberado en linea 258. buffer para el SCHC payload
            char* schc_message      = new char[payload_len + 1];    // liberado en linea 257. buffer para el SCHC message (header + payload)

            this->extractTiles(_currentTile_ptr, n_tiles_to_send, schc_payload);

            /* Crea un mensaje SCHC en formato hexadecimal */
            int schc_message_len = encoder.create_regular_fragment(_ruleID, _dTag, _currentWindow, _currentFcn, schc_payload, payload_len, schc_message);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHC_REGULAR_FRAGMENT_MSG, schc_message, schc_message_len); 

            /* Envía el mensaje a la capa 2*/
            uint8_t res = _stack->send_frame(_ruleID, schc_message, schc_message_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_message;
            delete[] schc_payload;

            _currentTile_ptr        = _currentTile_ptr + n_tiles_to_send;
            _currentFcn             = (_windowSize)-1;
            

            /* ******************* SCHC ALL-1 *********************************** */
            /* Se han enviado todos los full tiles de la sesion. Se envia un All-1*/
            if(_currentWindow == (_nWindows - 1))
            {
                /* buffer que almacena todos los tiles que se van a enviar */
                int payload_len                 = _lastTileSize;                    // tamaño del last tile en bytes
                char* schc_all_1_message    = new char[payload_len + 1 + 4];    // liberado en linea 285. buffer para el SCHC message: header (1B) + rcs (4B) + payload
                /* Crea un mensaje SCHC en formato hexadecimal */
                int schc_all_1_message_len = encoder.create_all_1_fragment(_ruleID, _dTag, _currentWindow, _rcs, _lastTile, payload_len, schc_all_1_message);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder.print_msg(SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message, schc_all_1_message_len); 

                /* Envía el mensaje a la capa 2*/
                uint8_t res = _stack->send_frame(_ruleID, schc_all_1_message, schc_all_1_message_len);
                if(res==1)
                {
                    SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                    return 1;
                }

                /* Eliminar los punteros a buffers*/
                delete[] schc_all_1_message;
            }

            _send_schc_ack_req_flag = true;    
        }
    }
    else if(_ackMode==ACK_MODE_ACK_END_SES || _ackMode==ACK_MODE_COMPOUND_ACK)
    {
        if(_send_schc_ack_req_flag == true)
        {
            /* ******************* Enviando SCHC ACK REQ ***************************** */
            /* Se envía un SCHC ACK REQ para empujar el envio en el downlink
            del SCHC ACK enviado por el SCHC Gateway */
            SCHC_Node_Message encoder_2;
            char* schc_ack_req_msg      = new char[1];      // liberado en linea 308
            int schc_ack_req_msg_len    = encoder_2.create_ack_request(_ruleID, _dTag, _currentWindow, schc_ack_req_msg);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder_2.print_msg(SCHC_ACK_REQ_MSG, schc_ack_req_msg, schc_ack_req_msg_len);

            /* Envía el mensaje a la capa 2*/
            int res = _stack->send_frame(_ruleID, schc_ack_req_msg, schc_ack_req_msg_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_ack_req_msg;

            _currentState           = STATE_TX_WAIT_x_ACK;
            _retrans_ack_req_flag   = true;
            _send_schc_ack_req_flag = false;
            
            SPDLOG_INFO("Changing STATE: From STATE_TX_SEND --> STATE_TX_WAIT_x_ACK");

            SPDLOG_TRACE("Leaving the function");
            
            return 0;
        }

        n_remaining_tiles = _nFullTiles - _currentTile_ptr; // numero de tiles de toda la sesion que faltan por enviar
        if(n_remaining_tiles > payload_available_in_tiles)
        {
            /* Si la cantidad de tiles que faltan por enviar 
            para la sesion no alcanza en la MTU significa 
            que aun NO finaliza la sesion
            */
            n_tiles_to_send = payload_available_in_tiles;

            /* buffer que almacena todos los tiles que se van a enviar */
            int payload_len         = n_tiles_to_send * _tileSize;  // tamaño del SCHC payload en bytes
            char* schc_payload      = new char[payload_len];        // liberado en linea 215. buffer para el SCHC payload
            char* schc_message      = new char[payload_len + 1];    // liberado en linea 214 buffer para el SCHC message (header + payload)            

            this->extractTiles(_currentTile_ptr, n_tiles_to_send, schc_payload);
            _currentTile_ptr    = _currentTile_ptr + n_tiles_to_send;

            /* Crea un mensaje SCHC en formato hexadecimal */
            int schc_message_len = encoder.create_regular_fragment(_ruleID, _dTag, _currentWindow, _currentFcn, schc_payload, payload_len, schc_message);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHC_REGULAR_FRAGMENT_MSG, schc_message, schc_message_len);

            /* Envía el mensaje a la capa 2*/
            uint8_t res = _stack->send_frame(_ruleID, schc_message, schc_message_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_message;
            delete[] schc_payload;

            
            if((_currentFcn - n_tiles_to_send <= -1))   // * detecta si se comenzaron a enviar los tiles de la siguiente ventana
            {
                _currentFcn = (_currentFcn - n_tiles_to_send) + _windowSize;
                _currentWindow = _currentWindow + 1;
            }
            else
            {
                _currentFcn = _currentFcn - n_tiles_to_send;
            }

        }
        else
        {
           /* Si la cantidad de tiles que faltan por enviar 
            para la sesion SI alcanza en la MTU 
            significa que este es el ultimo envio para la 
            sesion
            */
            n_tiles_to_send = n_remaining_tiles;

            /* buffer que almacena todos los tiles que se van a enviar */
            int payload_len         = n_tiles_to_send * _tileSize;  // tamaño del SCHC payload en bytes
            char* schc_payload      = new char[payload_len];        // liberado en linea 258. buffer para el SCHC payload
            char* schc_message      = new char[payload_len + 1];    // liberado en linea 257. buffer para el SCHC message (header + payload)

            this->extractTiles(_currentTile_ptr, n_tiles_to_send, schc_payload);
            _currentTile_ptr    = _currentTile_ptr + n_tiles_to_send;

            /* Crea un mensaje SCHC en formato hexadecimal */
            int schc_message_len = encoder.create_regular_fragment(_ruleID, _dTag, _currentWindow, _currentFcn, schc_payload, payload_len, schc_message);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHC_REGULAR_FRAGMENT_MSG, schc_message, schc_message_len); 

            /* Envía el mensaje a la capa 2*/
            uint8_t res = _stack->send_frame(_ruleID, schc_message, schc_message_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_message;
            delete[] schc_payload;

            
            /* ******************* SCHC ALL-1 *********************************** */
            /* Se envia el ultimo tile en un SCHC All-1 */
            if(_currentWindow == (_nWindows - 1))
            {
                /* buffer que almacena todos los tiles que se van a enviar */
                payload_len                 = _lastTileSize;                    // tamaño del last tile en bytes
                char* schc_all_1_message    = new char[payload_len + 1 + 4];    // liberado en linea 285. buffer para el SCHC message: header (1B) + rcs (4B) + payload
                
                /* Crea un mensaje SCHC en formato hexadecimal */
                int schc_all_1_message_len = encoder.create_all_1_fragment(_ruleID, _dTag, _currentWindow, _rcs, _lastTile, payload_len, schc_all_1_message);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder.print_msg(SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message, schc_all_1_message_len); 

                /* Envía el mensaje a la capa 2*/
                uint8_t res = _stack->send_frame(_ruleID, schc_all_1_message, schc_all_1_message_len);
                if(res==1)
                {
                    SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                    return 1;
                }

                /* Eliminar los punteros a buffers*/
                delete[] schc_all_1_message;
            }

            _send_schc_ack_req_flag = true;
        }        
    }

    SPDLOG_TRACE("Leaving the function");

    return 0;
}

uint8_t SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack(char *msg, int len)
{
    SPDLOG_TRACE("Entering the function");
    
    SCHC_Node_Message decoder;
    uint8_t msg_type = decoder.get_msg_type(SCHC_FRAG_LORAWAN, SCHC_FRAG_UPDIR_RULE_ID, msg, len);

    if(msg_type == SCHC_ACK_MSG)
    {
        SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack - Receiving a SCHC ACK msg");

        _retrans_ack_req_flag = false;

        if(_ackMode == ACK_MODE_ACK_END_WIN)
        {
            decoder.decodeMsg(SCHC_FRAG_LORAWAN, SCHC_FRAG_UPDIR_RULE_ID, msg, len, _bitmapArray);
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            
            if(c == 1 && w == (_nWindows-1))
            {
                /* SCHC ACK incluye bitmap sin errores y es un ACK a la ultima ventana*/
                decoder.print_msg(SCHC_ACK_MSG, msg, len, _bitmapArray);

                _currentWindow      = _currentWindow + 1; 
                _currentState = STATE_TX_END;
                
                SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_END");

            }
            else if(c == 1 && w != (_nWindows-1))
            {
                /* SCHC ACK incluye bitmap sin errores y NO es un ACK para la ultima ventana*/
                decoder.print_msg(SCHC_ACK_MSG, msg, len, _bitmapArray);

                _currentWindow      = _currentWindow + 1; 
                _currentState       = STATE_TX_SEND;
                
                SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_SEND");

            }
            else if(c == 0)
            {
                decoder.print_msg(SCHC_ACK_MSG, msg, len, _bitmapArray);
                _currentState       = STATE_TX_RESEND_MISSING_FRAG;
                
                SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
            }
        }
        else if(_ackMode == ACK_MODE_ACK_END_SES)
        {
            decoder.decodeMsg(SCHC_FRAG_LORAWAN, SCHC_FRAG_UPDIR_RULE_ID, msg, len, _bitmapArray);
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            _last_confirmed_window = w;

            if(c == 1)
            {
                /* SCHC ACK incluye bitmap sin errores y es un ACK a la ultima ventana*/
                decoder.print_msg(SCHC_ACK_MSG, msg, len, _bitmapArray);
                _currentState = STATE_TX_END;
                
                SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_END");
            }
            else
            {
                decoder.print_msg(SCHC_ACK_MSG, msg, len, _bitmapArray);
                _currentState       = STATE_TX_RESEND_MISSING_FRAG;
                
                SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
            }
        }
        else if(_ackMode == ACK_MODE_COMPOUND_ACK)
        {
            decoder.decodeMsg(SCHC_FRAG_LORAWAN, SCHC_FRAG_UPDIR_RULE_ID, msg, len, _bitmapArray);
            uint8_t c = decoder.get_c();
            uint8_t w = decoder.get_w();
            _win_with_errors.push_back(w);

            if(c == 1)
            {
                /* SCHC ACK incluye bitmap sin errores y es un ACK a la ultima ventana*/
                decoder.print_msg(SCHC_ACK_MSG, msg, len, _bitmapArray);
                _currentState = STATE_TX_END;
                
                SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_END");
            }
            else
            {
                decoder.print_msg(SCHC_ACK_MSG, msg, len, _bitmapArray);
                _currentState       = STATE_TX_RESEND_MISSING_FRAG;
                
                SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
            }
        }

        

    }
    else if(msg_type == SCHC_COMPOUND_ACK)
    {
        SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack - Receiving a SCHC Compound ACK msg");

        _retrans_ack_req_flag = false;

        decoder.decodeMsg(SCHC_FRAG_LORAWAN, SCHC_FRAG_UPDIR_RULE_ID, msg, len, _bitmapArray);
        uint8_t c           = decoder.get_c();
        _win_with_errors    = decoder.get_w_vector();

        if(c == 1)
        {
            /* SCHC ACK incluye bitmap sin errores y es un ACK a la ultima ventana*/
            decoder.print_msg(SCHC_COMPOUND_ACK, msg, len, _bitmapArray);
            _currentState   = STATE_TX_END;
            
            SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_END");
        }
        else
        {
            decoder.print_msg(SCHC_COMPOUND_ACK, msg, len, _bitmapArray);
            _currentState   = STATE_TX_RESEND_MISSING_FRAG;
            
            SPDLOG_INFO("Changing STATE: From STATE_TX_WAIT_x_ACK --> STATE_TX_RESEND_MISSING_FRAG");
        }

    }
    else if(msg_type == SCHC_RECEIVER_ABORT_MSG)
    {
            SPDLOG_TRACE("Receiving a SCHC Receiver-Abort message. Releasing resources...");

            // TODO: Liberar maquinas de estados y recursos de memoria. Se debe retornar la funcion SCHC_Node_Fragmenter::send()
    }

    SPDLOG_TRACE("Leaving the function");

    return 0;
}

uint8_t SCHC_Node_Ack_on_error::TX_END_free_resources()
{
    SPDLOG_TRACE("Entering the function");


    SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_END_free_resources - Releasing resources and finish machines and session");


 
    for(int i = 0 ; i < _nFullTiles ; i++ )
    {
        delete[] _tilesArray[i];
    }
    delete[] _tilesArray;
    delete[] _lastTile;

    _running = false;

    return 0;
}

uint8_t SCHC_Node_Ack_on_error::TX_RESEND_MISSING_FRAG_send_fragments(char *msg, int len)
{
    SPDLOG_TRACE("Entering the function");

    SCHC_Node_Message encoder;           // encoder

    // Buscar el primer cero y cuenta los ceros contiguos
    int adjacent_tiles      = 0;
    int bitmap_ptr          = -1;   // indice al primer zero del bitmap contando de izquierda a derecha
    int last_ptr            = -1;

    if(_ackMode==ACK_MODE_ACK_END_WIN)
    {
        /* Se envía un SCHC ACK REQ para empujar 
        el envio en el downlink del SCHC ACK enviado 
        por el SCHC Gateway */
        if(_send_schc_ack_req_flag == true)
        {
            if((_currentWindow == _nWindows-1) && _bitmapArray[_currentWindow][_windowSize-1] == 0)
            {
                /* Sending a SCHC All-1 fragment */
                int payload_len                 = _lastTileSize;                    // tamaño del last tile en bytes
                char* schc_all_1_message    = new char[payload_len + 1 + 4];        // liberado en linea 285. buffer para el SCHC message: header (1B) + rcs (4B) + payload
                int schc_all_1_message_len = encoder.create_all_1_fragment(_ruleID, _dTag, _currentWindow, _rcs, _lastTile, payload_len, schc_all_1_message);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder.print_msg(SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message, schc_all_1_message_len); 

                /* Envía el mensaje a la capa 2*/
                uint8_t res = _stack->send_frame(_ruleID, schc_all_1_message, schc_all_1_message_len);
                if(res==1)
                {
                    SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                    return 1;
                }

                /* Eliminar los punteros a buffers*/
                delete[] schc_all_1_message;                
            }

            _send_schc_ack_req_flag  = false;
            _retrans_ack_req_flag   = true;

            SCHC_Node_Message encoder_2;
            char* schc_ack_req_msg      = new char[1];      // liberado en linea 308
            int schc_ack_req_msg_len    = encoder_2.create_ack_request(_ruleID, 0, _currentWindow, schc_ack_req_msg);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder_2.print_msg(SCHC_ACK_REQ_MSG, schc_ack_req_msg, schc_ack_req_msg_len);

            _currentState   = STATE_TX_WAIT_x_ACK;

            SPDLOG_INFO("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_ACK");


            /* Envía el mensaje a la capa 2*/
            int res = _stack->send_frame(_ruleID, schc_ack_req_msg, schc_ack_req_msg_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_ack_req_msg;  

            return 0;         
        }

        if(_currentWindow == (_nWindows-1))
        {
            last_ptr =  _nFullTiles - (_nWindows - 1) * _windowSize;
        }
        else
        {
            last_ptr = _windowSize;
        }

        for (int i = 0; i < last_ptr; ++i) 
        {
            if (_bitmapArray[_currentWindow][i] == 0) 
            {
                if (bitmap_ptr == -1) 
                {
                    bitmap_ptr = i;     // Registrar la posición del primer cero
                }
                ++adjacent_tiles;       // Contar los ceros contiguos
            } 
            else if (bitmap_ptr != -1) 
            {
                break;                  // Salir del bucle después de contar los ceros contiguos
            }
        }

        /* Numero de tiles que se pueden enviar un un payload */
        int payload_available_in_bytes = _current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
        int payload_available_in_tiles = payload_available_in_bytes/_tileSize;

        int n_tiles_to_send     = 0;    // numero de tiles a enviar
        if(adjacent_tiles > payload_available_in_tiles)
        {
            n_tiles_to_send = payload_available_in_tiles;
        }
        else
        {
            n_tiles_to_send = adjacent_tiles;
        }
            

        if(n_tiles_to_send != 0)
        {
            /* buffer que almacena todos los tiles que se van a enviar */
            int payload_len         = n_tiles_to_send * _tileSize;  // tamaño del SCHC payload en bytes
            char* schc_payload      = new char[payload_len];        // liberado en linea 657. buffer para el SCHC payload
            char* schc_message      = new char[payload_len + 1];    // liberado en linea 658. buffer para el SCHC message (header + payload)            
            
            int currentTile_ptr = getCurrentTile_ptr(_currentWindow, bitmap_ptr);
            int currentFcn      = get_current_fcn(bitmap_ptr);

            this->extractTiles(currentTile_ptr, n_tiles_to_send, schc_payload);

            /* Crea un mensaje SCHC en formato hexadecimal */
            int schc_message_len = encoder.create_regular_fragment(_ruleID, 0, _currentWindow, currentFcn, schc_payload, payload_len, schc_message);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHC_REGULAR_FRAGMENT_MSG, schc_message, schc_message_len);

            /* Envía el mensaje a la capa 2*/
            uint8_t res = _stack->send_frame(_ruleID, schc_message, schc_message_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_payload;
            delete[] schc_message;

            /* Marca con 1 los tiles que se han retransmitido */
            for(int i = bitmap_ptr; i < (bitmap_ptr + n_tiles_to_send); i++)
            {
                _bitmapArray[_currentWindow][i] = 1;
            }


        }


        /* Revisa si hay mas tiles perdidos*/
        int ones = 0;
        for(int i = 0; i<last_ptr; i++)
        {
            if(_bitmapArray[_currentWindow][i] == 0)
            {
                _send_schc_ack_req_flag = false;
                break;
            }
            else
            {
                ones = ones + _bitmapArray[_currentWindow][i];
            }

            if(ones == last_ptr)
            {
                _send_schc_ack_req_flag = true;
                break;
            }
        }

    }
    else if(_ackMode==ACK_MODE_ACK_END_SES)
    {
        if(_send_schc_ack_req_flag == true)
        {

            if((_last_confirmed_window == _nWindows-1) && _bitmapArray[_currentWindow][_windowSize-1] == 0)
            {
                /* Sending a SCHC All-1 fragment */
                SCHC_Node_Message    encoder_all_1;
                int payload_len             = _lastTileSize;                    // tamaño del last tile en bytes
                char* schc_all_1_message    = new char[payload_len + 1 + 4];        // liberado en linea 285. buffer para el SCHC message: header (1B) + rcs (4B) + payload
                int schc_all_1_message_len  = encoder_all_1.create_all_1_fragment(_ruleID, _dTag, _currentWindow, _rcs, _lastTile, payload_len, schc_all_1_message);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder_all_1.print_msg(SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message, schc_all_1_message_len); 

                /* Envía el mensaje a la capa 2*/
                uint8_t res = _stack->send_frame(_ruleID, schc_all_1_message, schc_all_1_message_len);
                if(res==1)
                {
                    SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                    return 1;
                }

                /* Eliminar los punteros a buffers*/
                delete[] schc_all_1_message;                
            }



            /* Se envía un SCHC ACK REQ para empujar el envio en el downlink
            del SCHC ACK enviado por el SCHC Gateway */
            SCHC_Node_Message encoder_ack_req;
            char* schc_ack_req_msg      = new char[1];      // liberado en linea 308
            int schc_ack_req_msg_len    = encoder_ack_req.create_ack_request(_ruleID, 0, _last_confirmed_window, schc_ack_req_msg);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder_ack_req.print_msg(SCHC_ACK_REQ_MSG, schc_ack_req_msg, schc_ack_req_msg_len);

            /* Envía el mensaje a la capa 2*/
            int res = _stack->send_frame(_ruleID, schc_ack_req_msg, schc_ack_req_msg_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_ack_req_msg;

            _retrans_ack_req_flag       = true;
            _send_schc_ack_req_flag     = false;
            _currentState   = STATE_TX_WAIT_x_ACK;
            
            SPDLOG_INFO("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_ACK");

            SPDLOG_TRACE("Leaving the function");

            return 0;            
        }


        /* Determina cual es el ultimo tile de la ventana */
        if(_last_confirmed_window == (_nWindows-1))
        {
            last_ptr =  _nFullTiles - (_nWindows - 1) * _windowSize;
        }
        else
        {
            last_ptr = _windowSize;
        } 

        /* Cuenta el numero de tiles perdidos contiguos */
        for (int i = 0; i < last_ptr; ++i) 
        {
            if (_bitmapArray[_last_confirmed_window][i] == 0) 
            {
                if (bitmap_ptr == -1) 
                {
                    bitmap_ptr = i;     // Registrar la posición del primer cero
                }
                ++adjacent_tiles;       // Contar los ceros contiguos
            } 
            else if (bitmap_ptr != -1) 
            {
                break;                  // Salir del bucle después de contar los ceros contiguos
            }
        }


        /* Numero de tiles que se pueden enviar un un payload */
        int payload_available_in_bytes = _current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
        int payload_available_in_tiles = payload_available_in_bytes/_tileSize;

        int n_tiles_to_send     = 0;    // numero de tiles a enviar
        if(adjacent_tiles > payload_available_in_tiles)
        {
            n_tiles_to_send = payload_available_in_tiles;
        }
        else
        {
            n_tiles_to_send = adjacent_tiles;
        }

        if(n_tiles_to_send != 0)
        {

            /* buffer que almacena todos los tiles que se van a enviar */
            int payload_len         = n_tiles_to_send * _tileSize;  // tamaño del SCHC payload en bytes
            char* schc_payload      = new char[payload_len];        // liberado en linea 657. buffer para el SCHC payload
            char* schc_message      = new char[payload_len + 1];    // liberado en linea 658. buffer para el SCHC message (header + payload)            
            
            int currentTile_ptr = getCurrentTile_ptr(_last_confirmed_window, bitmap_ptr);
            int currentFcn      = get_current_fcn(bitmap_ptr);

            this->extractTiles(currentTile_ptr, n_tiles_to_send, schc_payload);

            /* Crea un mensaje SCHC en formato hexadecimal */
            int schc_message_len = encoder.create_regular_fragment(_ruleID, 0, _last_confirmed_window, currentFcn, schc_payload, payload_len, schc_message);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHC_REGULAR_FRAGMENT_MSG, schc_message, schc_message_len);

            /* Envía el mensaje a la capa 2*/
            uint8_t res = _stack->send_frame(_ruleID, schc_message, schc_message_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_payload;
            delete[] schc_message;

            /* Marca con 1 los tiles que se han retransmitido */
            for(int i = bitmap_ptr; i < (bitmap_ptr + n_tiles_to_send); i++)
            {
                _bitmapArray[_last_confirmed_window][i] = 1;
            }

        }

        /* Revisa si hay tiles perdidos. Si los hay, vuelve a llamar a este mismo metodo*/
        int c = 1;
        for(int i = 0; i<last_ptr; i++)
        {
            if(_bitmapArray[_last_confirmed_window][i] == 0)
            {
                c = 0;
                break;
            }
        }

        if(c == 1)
        {
            /* NO  hay tiles perdidos. Se marca flag en true y se vuelve a llamar este mismo metodo con para enviar un SCHC ACK REQ*/
            _send_schc_ack_req_flag = true;
        }
    }
    else if(_ackMode==ACK_MODE_COMPOUND_ACK)
    {

        /* Extrae la ventana a la que se deben enviar los tails*/
        if(!_win_with_errors.empty())
            _last_confirmed_window = _win_with_errors.front();

        /* Se envía un SCHC ACK REQ para empujar el envio en 
        el downlink del SCHC ACK enviado por el SCHC Gateway */
        if(_send_schc_ack_req_flag == true)
        {

            if((_last_confirmed_window == _nWindows-1) && _bitmapArray[_currentWindow][_windowSize-1] == 0)
            {
                /* Sending a SCHC All-1 fragment */
                SCHC_Node_Message    encoder_all_1;
                int payload_len             = _lastTileSize;                    // tamaño del last tile en bytes
                char* schc_all_1_message    = new char[payload_len + 1 + 4];    // liberado en linea 285. buffer para el SCHC message: header (1B) + rcs (4B) + payload
                int schc_all_1_message_len  = encoder_all_1.create_all_1_fragment(_ruleID, _dTag, _currentWindow, _rcs, _lastTile, payload_len, schc_all_1_message);

                /* Imprime los mensajes para visualizacion ordenada */
                encoder_all_1.print_msg(SCHC_ALL1_FRAGMENT_MSG, schc_all_1_message, schc_all_1_message_len); 

                /* Envía el mensaje a la capa 2*/
                uint8_t res = _stack->send_frame(_ruleID, schc_all_1_message, schc_all_1_message_len);
                if(res==1)
                {
                    SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                    return 1;
                }

                /* Eliminar los punteros a buffers*/
                delete[] schc_all_1_message;                
            }



            SCHC_Node_Message encoder_2;
            char* schc_ack_req_msg      = new char[1];      // liberado en linea 308
            int schc_ack_req_msg_len    = encoder_2.create_ack_request(_ruleID, 0, _last_confirmed_window, schc_ack_req_msg);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder_2.print_msg(SCHC_ACK_REQ_MSG, schc_ack_req_msg, schc_ack_req_msg_len);

            /* Envía el mensaje a la capa 2*/
            int res = _stack->send_frame(_ruleID, schc_ack_req_msg, schc_ack_req_msg_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_ack_req_msg;

            _retrans_ack_req_flag       = true;
            _send_schc_ack_req_flag     = false;
            _currentState   = STATE_TX_WAIT_x_ACK;
            
            SPDLOG_INFO("Changing STATE: From STATE_TX_RESEND_MISSING_FRAG --> STATE_TX_WAIT_x_ACK");

            SPDLOG_TRACE("Leaving the function");

            return 0;            
        }

        /* Determina cual es el ultimo tile de la ventana */
        if(_last_confirmed_window == (_nWindows-1))
        {
            last_ptr =  _nFullTiles - (_nWindows - 1) * _windowSize;
        }
        else
        {
            last_ptr = _windowSize;
        } 

       /* Cuenta el numero de tiles perdidos contiguos */
        for (int i = 0; i < last_ptr; ++i) 
        {
            if (_bitmapArray[_last_confirmed_window][i] == 0) 
            {
                if (bitmap_ptr == -1) 
                {
                    bitmap_ptr = i;     // Registrar la posición del primer cero
                }
                ++adjacent_tiles;       // Contar los ceros contiguos
            } 
            else if (bitmap_ptr != -1) 
            {
                break;                  // Salir del bucle después de contar los ceros contiguos
            }
        }


        /* Numero de tiles que se pueden enviar un un payload */
        int payload_available_in_bytes = _current_L2_MTU - 1; // MTU = SCHC header + SCHC payload
        int payload_available_in_tiles = payload_available_in_bytes/_tileSize;

        int n_tiles_to_send     = 0;    // numero de tiles a enviar
        if(adjacent_tiles > payload_available_in_tiles)
        {
            n_tiles_to_send = payload_available_in_tiles;
        }
        else
        {
            n_tiles_to_send = adjacent_tiles;
        }

         if(n_tiles_to_send != 0)
         {       
            /* buffer que almacena todos los tiles que se van a enviar */
            int payload_len         = n_tiles_to_send * _tileSize;  // tamaño del SCHC payload en bytes
            char* schc_payload      = new char[payload_len];        // liberado en linea 657. buffer para el SCHC payload
            char* schc_message      = new char[payload_len + 1];    // liberado en linea 658. buffer para el SCHC message (header + payload)            
            
            int currentTile_ptr = getCurrentTile_ptr(_last_confirmed_window, bitmap_ptr);
            int currentFcn      = get_current_fcn(bitmap_ptr);

            this->extractTiles(currentTile_ptr, n_tiles_to_send, schc_payload);

            /* Crea un mensaje SCHC en formato hexadecimal */
            int schc_message_len = encoder.create_regular_fragment(_ruleID, 0, _last_confirmed_window, currentFcn, schc_payload, payload_len, schc_message);

            /* Imprime los mensajes para visualizacion ordenada */
            encoder.print_msg(SCHC_REGULAR_FRAGMENT_MSG, schc_message, schc_message_len);

            /* Envía el mensaje a la capa 2*/
            uint8_t res = _stack->send_frame(_ruleID, schc_message, schc_message_len);
            if(res==1)
            {
                SPDLOG_TRACE("SCHC_Node_Ack_on_error::sendRegularFragment - ERROR sending L2 frame");
                return 1;
            }

            /* Eliminar los punteros a buffers*/
            delete[] schc_payload;
            delete[] schc_message;

            /* Marca con 1 los tiles que se han retransmitido */
            for(int i = bitmap_ptr; i < (bitmap_ptr + n_tiles_to_send); i++)
            {
                _bitmapArray[_last_confirmed_window][i] = 1;
            }
        }

        /* Revisa si hay mas tiles perdidos. Si los hay, vuelve a llamar a este mismo metodo*/
        int c = 1;
        for(int i = 0; i<last_ptr; i++)
        {
            if(_bitmapArray[_last_confirmed_window][i] == 0)
            {
                c = 0;
                break;
            }
        }

        if(c == 1)
        {
            /* No hay mas tiles perdidos en la ventana actual.
            Se elimina la ventana corregida del vector _win_with_errors. 
            Se vuelve a llamar a este mismo metodo */
            if(!_win_with_errors.empty())
                _win_with_errors.erase(_win_with_errors.begin());

            if(_win_with_errors.empty())
                _send_schc_ack_req_flag = true;

        }
    }

    SPDLOG_TRACE("Leaving the function");

    return 0;
}

uint8_t SCHC_Node_Ack_on_error::mtuUpgrade(int mtu)
{
    SPDLOG_TRACE("Entering the function");


    SPDLOG_TRACE("SCHC_Node_Ack_on_error::mtuUpgrade - updating the MTU to: ");
    SPDLOG_TRACE(mtu);
    SPDLOG_TRACE(" bytes");

    _current_L2_MTU = mtu;

    SPDLOG_TRACE("Leaving the function");

    return 0;
}

uint8_t SCHC_Node_Ack_on_error::divideInTiles(char *buffer, int len)
{
/* Creates an array of size _nFullTiles x _tileSize to store the message. 
Each row of the array is a tile of tileSize bytes. It also determines 
the number of SCHC windows.*/

    SPDLOG_TRACE("Entering the function");

/* RFC9011
 5.6.2. Uplink Fragmentation: From Device to SCHC Gateway

Last tile: It can be carried in a Regular SCHC Fragment, alone 
in an All-1 SCHC Fragment, or with any of these two methods. 
Implementations must ensure that:
*/
    if((len%_tileSize)==0)
    {
        /* El ultimo tile es del tamaño de _TileSize */
        _nFullTiles = (len/_tileSize)-1;
        _lastTileSize = _tileSize;  
    }
    else
    {
        /* El ultimo tile es menor _TileSize */
        _nFullTiles = (len/_tileSize);
        _lastTileSize = len%_tileSize;
    }


    SPDLOG_TRACE("SCHC_Node_Ack_on_error::divideInTiles - Full Tiles number: ");
    SPDLOG_TRACE(_nFullTiles);
    SPDLOG_TRACE(" tiles");
    SPDLOG_TRACE("SCHC_Node_Ack_on_error::divideInTiles - Last Tile Size: ");
    SPDLOG_TRACE(_lastTileSize);
    SPDLOG_TRACE(" bytes");


    // memory allocated for elements of rows.
    _tilesArray = new char*[_nFullTiles];           // Liberado en metodo SCHC_Node_Ack_on_error::TX_END_free_resources()

    // memory allocated for  elements of each column.  
    for(int i = 0 ; i < _nFullTiles ; i++ )
    {
        _tilesArray[i] = new char[_tileSize];       // Liberado en metodo SCHC_Node_Ack_on_error::TX_END_free_resources()
    }

    int k=0;
    for(int i=0; i<_nFullTiles; i++)
    {
        for(int j=0; j<_tileSize;j++)
        {
            _tilesArray[i][j] = buffer[k];
            k++;
        }
    }

    _lastTile = new char[_lastTileSize];            // Liberado en metodo SCHC_Node_Ack_on_error::TX_END_free_resources()
    for(int i=0; i<_lastTileSize; i++)
    {
        _lastTile[i] = buffer[k];
        k++;
    }

    /* Numero de ventanas SCHC */
    if(len>(_tileSize*_windowSize*3))
    {
        _nWindows = 4;
    }
    else if(len>(_tileSize*_windowSize*2))
    {
        _nWindows = 3;
    }
    else if (len>(_tileSize*_windowSize))
    {
        _nWindows = 2;
    }
    else
    {
        _nWindows = 1;
    }

    SPDLOG_TRACE("SCHC_Node_Ack_on_error::divideInTiles - number of SCHC Windows: ");
    SPDLOG_TRACE(_nWindows);


    //this->printTileArray();

    /* Calculo del RCS de todo el mensaje. Será usado en el envío de un SCHC All-1 fragment*/
    _rcs = this->calculate_crc32(buffer, len);

    SPDLOG_TRACE("SCHC_Node_Ack_on_error::divideInTiles - RCS before to send: ");
    SPDLOG_TRACE(_rcs);



    SPDLOG_TRACE("Leaving the function");

    return 0;
}

uint8_t SCHC_Node_Ack_on_error::extractTiles(uint8_t firstTileID, uint8_t nTiles, char *buff)
{

    SPDLOG_TRACE("Entering the function");

    int k=0;
    for(int i=firstTileID; i<(firstTileID+nTiles); i++)
    {
        for(int j=0;j<_tileSize;j++)
        {
            buff[k] = _tilesArray[i][j];
            k++;
        }
    }

    SPDLOG_TRACE("Leaving the function");

    return 0;
}

int SCHC_Node_Ack_on_error::getCurrentTile_ptr(int window, int bitmap_ptr)
{
    return (window * _windowSize) + bitmap_ptr;
}

uint8_t SCHC_Node_Ack_on_error::get_current_fcn(int bitmap_ptr)
{
    return (_windowSize - 1) - bitmap_ptr;
}

void SCHC_Node_Ack_on_error::printTileArray()
{
    SPDLOG_TRACE("***********************************************");
    for(int i=0; i<_nFullTiles; i++)
    {
        if(i+1<10)
        {
            SPDLOG_TRACE("Tile 0");
            SPDLOG_TRACE(i+1);
            SPDLOG_TRACE(" --> ");
        }
        else
        {
            SPDLOG_TRACE("Tile ");
            SPDLOG_TRACE(i+1);
            SPDLOG_TRACE(" --> ");
        }
        for(int j=0; j<_tileSize;j++)
        {
            SPDLOG_TRACE(_tilesArray[i][j]);
            SPDLOG_TRACE(" ");       
        }
        SPDLOG_TRACE();
    }

    if(_nFullTiles+1<10)
    {
        SPDLOG_TRACE("Tile 0");
        SPDLOG_TRACE(_nFullTiles+1);
        SPDLOG_TRACE(" --> ");
    }
    else
    {
        SPDLOG_TRACE("Tile ");
        SPDLOG_TRACE(_nFullTiles+1);
        SPDLOG_TRACE(" --> ");
    }
    for(int i=0; i<_lastTileSize; i++)
    {
        SPDLOG_TRACE(_lastTile[i]);
        SPDLOG_TRACE(" ");       
    }
    SPDLOG_TRACE();
    SPDLOG_TRACE("***********************************************");
}

uint32_t SCHC_Node_Ack_on_error::calculate_crc32(const char *data, size_t length) 
{
    // Polinomio CRC32 (reflejado)
    const uint32_t polynomial = 0xEDB88320;
    uint32_t crc = 0xFFFFFFFF;

    // Procesar cada byte en el buffer
    for (size_t i = 0; i < length; i++) {
        crc ^= static_cast<uint8_t>(data[i]); // Asegúrate de que el dato sea tratado como uint8_t

        // Procesar los 8 bits del byte
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

void SCHC_Node_Ack_on_error::send_ack_req()
{

    SPDLOG_TRACE("SCHC_Node_Ack_on_error::send_ack_req - Sending SCHC ACK Req");

    SCHC_Node_Message encoder;
    char schc_ack_req_msg[1];
    int schc_ack_req_msg_len    = encoder.create_ack_request(_ruleID, _dTag, _currentWindow, schc_ack_req_msg);

    /* Imprime los mensajes para visualizacion ordenada */
    encoder.print_msg(SCHC_ACK_REQ_MSG, schc_ack_req_msg, schc_ack_req_msg_len);

    /* Envía el mensaje a la capa 2*/
    int res = _stack->send_frame(_ruleID, schc_ack_req_msg, schc_ack_req_msg_len);
    if(res==1)
    {
        SPDLOG_TRACE("SCHC_Node_Ack_on_error::TX_WAIT_x_ACK_receive_ack - ERROR sending L2 frame");
        return;
    }

    _finish_loop_ack        = true;
    _retrans_ack_req_flag   = true;
    _retransTimer_counter++;
    

    SPDLOG_TRACE("Leaving the function");
}

void SCHC_Node_Ack_on_error::send_sender_abort()
{

    SPDLOG_TRACE("SCHC_Node_Ack_on_error::send_sender_abort - Sending SCHC Sender-Abort");

}

void finish_loop()
{
    _finish_loop = false;
}

void finish_loop_ack()
{
    _finish_loop_ack = false;
}