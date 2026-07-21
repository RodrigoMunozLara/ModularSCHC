#include "stacks/SCHCLoRaWANStack.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "spdlog/spdlog.h"

SCHCLoRaWANStack::SCHCLoRaWANStack(AppConfig& appConfig, SCHCCore& schcCore): _appConfig(appConfig), _schcCore(schcCore)
{
    SPDLOG_DEBUG("Executing SCHCLoRaWANStack constructor()");

    _keep_reading = true;

    /* ***** Configuring serial connection **** */
    std::string _port = _appConfig.lorawan_node.serial_port;
    _fd = open(_port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (_fd == -1) {
        SPDLOG_ERROR("The port could not be opened: {}", _port);
    }

    // Termios configuration
    struct termios tty;
    if (tcgetattr(_fd, &tty) != 0) 
    {
        close(_fd);
        SPDLOG_ERROR("Error obtaining port attributes. Closing File descriptor");
    }

    // Baud rate set to 115200 (modem standard)
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag |= (CLOCAL | CREAD);    // Ignore control lines, enable reading
    tty.c_cflag &= ~PARENB;             // No parity
    tty.c_cflag &= ~CSTOPB;             // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;                 // 8 bits

    // RAW mode: does not process special characters
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    // Configuring read timeouts
    tty.c_cc[VMIN] = 0;     // Non-blocking reading
    tty.c_cc[VTIME] = 10;   // 1 second timeout for each read

    if (tcsetattr(_fd, TCSANOW, &tty) != 0) {
        close(_fd);
        SPDLOG_ERROR("Error configuring the port. Closing File descriptor");
    }
    SPDLOG_DEBUG("Successfully connected to {} port", _port);

    /* ***** Configuring LoRaWAN device ***** */
    
    // Iniciamos el hilo lector al final del constructor, una vez configurado el puerto
    _serial_reader_thread = std::thread(&SCHCLoRaWANStack::serial_reader_loop, this);
    SPDLOG_DEBUG("Waiting 2000 ms.....");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::string resp = send_command("AT+VER=?");
    SPDLOG_DEBUG("AT+VER=? {}", resp);

    // resp = send_command("AT+NJM=1");   /* Network Join Mode: AT+NJM: get or set the network join mode (0 = ABP, 1 = OTAA)*/
    // SPDLOG_DEBUG("AT+NJM=1 {}", resp);

    resp = send_command("AT+CLASS=A");   /* LoRa Class: AT+CLASS: get or set the device class (A = class A, B = class B, C = class C)*/
    SPDLOG_DEBUG("AT+CLASS=A {}", resp);

    resp = send_command("AT+ADR=0"); /* Adaptive Rate: AT+ADR: get or set the adaptive data rate setting (0 = OFF, 1 = ON)*/
    SPDLOG_DEBUG("AT+ADR=0 {}", resp);

    resp = send_command("AT+DCS=0"); /* Duty cycle settings: AT+DCS: get or set the ETSI duty cycle setting (0 = disabled, 1 = enabled) */
    SPDLOG_DEBUG("AT+DCS=0 {}", resp);

    resp = send_command("AT+TXP=5"); /* Transmit power: AT+TXP: get or set the transmitting power (0 = highest TX power, 10 = lowest TX power)*/
    SPDLOG_DEBUG("AT+TXP=5 {}", resp);

    resp = send_command("AT+BAND=6");    /* Active region: AT+BAND: get or set the active region (0 = EU433, 1 = CN470, 2 = RU864, 3 = IN865, 4 = EU868, 5 = US915, 6 = AU915, 7 = KR920, 8 = AS923-1, 9 = AS923-2, 10 = AS923-3, 11 = AS923-4, 12 = LA915)*/
    SPDLOG_DEBUG("AT+BAND=6 {}", resp);

    resp = send_command("AT+CHE=2"); /*  Eight channel mode: AT+CHE: get or set eight channels mode (only for US915 AU915 LA915 CN470)*/
    SPDLOG_DEBUG("AT+CHE=2 {}", resp);

    resp = send_command("AT+CFM=0"); /* Confirm Mode: AT+CFM: get or set the confirmation mode (0 = OFF, 1 = ON)*/
    SPDLOG_DEBUG("AT+CFM=0 {}", resp);

    resp = send_command("AT+RX1DL=1"); /* get or set the delay between the end of TX and the RX window 1 in seconds (1-15)*/
    SPDLOG_DEBUG("AT+RX1DL=1 {}", resp);

    resp = send_command("AT+RX2DL=2"); /* get or set the delay between the end of TX and the RX window 2 in seconds (2-16)*/
    SPDLOG_DEBUG("AT+RX2DL=2 {}", resp);


    std::string _deveui = "AT+DEVEUI=" + _appConfig.lorawan_node.deveui;
    resp = send_command(_deveui);
    SPDLOG_DEBUG("{} {}", _deveui, resp);

    std::string _appeui = "AT+APPEUI=" + _appConfig.lorawan_node.appeui;
    resp = send_command(_appeui);
    SPDLOG_DEBUG("{} {}", _appeui, resp);

    std::string _appkey = "AT+APPKEY=" + _appConfig.lorawan_node.appkey;
    resp = send_command(_appkey);
    SPDLOG_DEBUG("{} {}", _appkey, resp);


    std::string _njs = "AT+NJS=?";
    resp = send_command(_njs);
    SPDLOG_DEBUG("{} {}", _njs, resp);
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    SPDLOG_DEBUG("Waiting 3000 ms.....");
    if (!_joined)
    {
        std::string _njs_1 = "AT+JOIN=1";
        resp = send_command(_njs_1);  /* Join LoRaWAN Network: AT+JOIN: join network*/
        SPDLOG_DEBUG("{} {}", _njs_1, resp);
    } 



}

SCHCLoRaWANStack::~SCHCLoRaWANStack()
{
SPDLOG_DEBUG("Executing SCHCLoRaWANStack destructor()");
    
    // Primero avisamos al hilo que debe terminar
    _keep_reading = false;
    
    // Esperamos de forma segura a que el hilo muera
    if (_serial_reader_thread.joinable()) {
        _serial_reader_thread.join();
    }

    // Ya sin hilos secundarios vivos, enviamos el comando de salida y cerramos
    std::string resp = send_command("AT+JOIN=0");  
    SPDLOG_DEBUG("AT+JOIN=0 {}", resp);
    
    if (_fd != -1) {
        close(_fd);
        _fd = -1;
    }
}

void SCHCLoRaWANStack::init()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(6000));
    if(_appConfig.lorawan_node.data_rate.compare("DR0") == 0) _dr = 0;
    else if (_appConfig.lorawan_node.data_rate.compare("DR1") == 0) _dr = 1;
    else if (_appConfig.lorawan_node.data_rate.compare("DR2") == 0) _dr = 2;
    else if (_appConfig.lorawan_node.data_rate.compare("DR3") == 0) _dr = 3;
    else if (_appConfig.lorawan_node.data_rate.compare("DR4") == 0) _dr = 4;
    else if (_appConfig.lorawan_node.data_rate.compare("DR5") == 0) _dr = 5;

        /* Data rate: AT+DR: get or set the data rate */
    std::string resp;
    if(_dr==0) resp = send_command("AT+DR=0");
    else if(_dr==1) resp = send_command("AT+DR=1");
    else if(_dr==2) resp = send_command("AT+DR=2");
    else if(_dr==3) resp = send_command("AT+DR=3");
    else if(_dr==4) resp = send_command("AT+DR=4");
    else if(_dr==5) resp = send_command("AT+DR=5");
    SPDLOG_DEBUG("AT+DR:{} {}", _dr, resp);


    if(_appConfig.lorawan_node.node_class.compare("C") == 0)
    {
        resp = send_command("AT+CLASS=C");   /* LoRa Class: AT+CLASS: get or set the device class (A = class A, B = class B, C = class C)*/
        SPDLOG_DEBUG("AT+CLASS=C {}", resp);
    }

    resp = send_command("AT+CFM=0"); /* Confirm Mode: AT+CFM: get or set the confirmation mode (0 = OFF, 1 = ON)*/
    SPDLOG_DEBUG("AT+CFM=0 {}", resp);
}

std::string SCHCLoRaWANStack::send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId)
{
/* Creating the AT command */
    std::stringstream ss;
    ss << "AT+SEND=" << ruleid << ":" << toHexString(buff);
    std::string command = ss.str();
    SPDLOG_DEBUG("{}", command);

    int timeoutMs = 3000;
    std::string res = send_command(command);
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));

    return res;
}

std::string SCHCLoRaWANStack::send_command(const std::string &command, int timeoutMs)
{
    // Evitamos que dos hilos escriban al mismo tiempo en el puerto serie
    std::unique_lock<std::mutex> writeLock(_serial_write_mtx);
    
    // Preparamos el entorno para esperar la respuesta
    {
        std::unique_lock<std::mutex> cmdLock(_cmd_mtx);
        _cmd_ready = false;
        _cmd_response.clear();
    }

    std::string fullCmd = command + "\r\n";
    //SPDLOG_DEBUG("Writing AT Command: {}", command);
    
    if (write(_fd, fullCmd.c_str(), fullCmd.length()) < 0) {
        return "ERROR_WRITE";
    }
    
    // Ya escribimos, podemos soltar el lock de escritura por si otro hilo quiere encolar comandos
    writeLock.unlock(); 

    // Esperamos a que el hilo lector capture la respuesta en el buffer
    std::unique_lock<std::mutex> cmdLock(_cmd_mtx);
    bool success = _cmd_cv.wait_for(cmdLock, std::chrono::milliseconds(timeoutMs), [this]() {
        return _cmd_ready;
    });

    if (!success) {
        SPDLOG_WARN("Timeout waiting for command response: {}", command);
        return "TIMEOUT_DATA";
    }

    return _cmd_response;
}

void SCHCLoRaWANStack::receive_handler(const std::vector<uint8_t>& frame)
{
    auto msgStack       = std::make_unique<StackMessage>();
    
    SPDLOG_DEBUG("Message received: {:02x}", fmt::join(frame, " "));

    uint8_t ruleID = frame.front();
    std::vector<uint8_t> payload(frame.begin() + 1, frame.end());

    msgStack->ruleId = ruleID;
    msgStack->payload = payload;
    msgStack->len = payload.size();
    msgStack->deviceId = "";

    _schcCore.enqueueFromStack(std::move(msgStack));

}

uint32_t SCHCLoRaWANStack::getMtu()
{
    int fOpt = 0;   
    bool consider_Fopt = true;
    bool consider_Dwell = true;

    if(consider_Fopt)
    {
        fOpt = 15;  // 15 bytes is the max
    }
    
    if(consider_Dwell)
    {
        if(_dr==0 || _dr==1 || _dr==2) return 0;
        else if(_dr==3) return 53 - fOpt;
        else if(_dr==4) return 125 - fOpt;
        else if(_dr==5) return 242 - fOpt;
    }
    else
    {
        if(_dr==0 || _dr==1 || _dr==2) return 51 - fOpt;
        else if(_dr==3) return 115 - fOpt;
        else if(_dr==4 || _dr==5) return 222 - fOpt;
    }


    return -1;
}

std::string SCHCLoRaWANStack::toHexString(const std::vector<uint8_t> &data)
{
    std::stringstream ss;
    // Configuramos el stream una sola vez
    ss << std::hex << std::setfill('0') << std::uppercase;
    
    // Usamos un range-based for loop, que es más limpio y evita errores de índices
    for (const auto& byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    
    return ss.str();
}

std::vector<uint8_t> SCHCLoRaWANStack::convertUnicastToBytes(const std::string& input) {
    std::string target = "UNICAST:";
    size_t pos = input.find(target);
    if (pos == std::string::npos) return {};

    std::string result = input.substr(pos + target.length());
    std::vector<uint8_t> bytes;
    
    // 1. Buscamos el separador ':'
    size_t colonPos = result.find(':');
    if (colonPos == std::string::npos) return bytes;

    // 2. Procesamos el RuleID (antes del ':') - Base 10
    std::string firstPart = result.substr(0, colonPos);
    if (!firstPart.empty()) {
        bytes.push_back(static_cast<uint8_t>(std::stoi(firstPart)));
    }

    // 3. Procesamos el Payload (después del ':') - Base 16
    std::string secondPart = result.substr(colonPos + 1);
    bytes.reserve(1 + (secondPart.length() / 2));

    for (size_t i = 0; i < secondPart.length(); i += 2) {
        std::string byteString = secondPart.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoul(byteString, nullptr, 16)));
    }

    return bytes;
}

void SCHCLoRaWANStack::serial_reader_loop()
{
    SPDLOG_DEBUG("Starting background thread to read serial port");
    std::string accumulator = "";
    char readBuffer[256];

    while (_keep_reading) 
    {
        // Lectura bloqueante o con timeout corto (VTIME=1), sin mutex que lo detenga
        int bytesRead = read(_fd, readBuffer, sizeof(readBuffer) - 1);
        if (bytesRead > 0) 
        {
            readBuffer[bytesRead] = '\0';
            accumulator += readBuffer;
            //print_buffer_hex(readBuffer);

            // https://docs.rakwireless.com/product-categories/software-apis-and-libraries/rui3/at-command-manual/#rui3-at-command-format
            // La documentacion de RAK indica que la salida de los comandos AT tiene la forma:
            // AT+XXX=<value><CR><LF>
            // <CR><LF><Status<CR><LF>
            //
            // En realidad son:
            // AT+XXX=<value><LF><LF>
            // <LF><LF><Status<LF><LF>
            //
            // <CR>: \r
            // <LF>: \n
            // Procesar hasta que encuentra (\n\n).  
            size_t pos;
            while ((pos = accumulator.find("\n\n")) != std::string::npos) 
            {
                std::string line = accumulator.substr(0, pos);
                accumulator.erase(0, pos + 2); // Remover del acumulador la data que se va a procesar mas abajo

                if (line.empty()) continue; // El comando recibido tiene la forma <LF><LF><Status<LF><LF>. Retiro los dos <LF><LF> del comienzo

                // Caso 1: Se recibe algun status 
                if (line.find("OK") != std::string::npos || line.find("AT_ERROR") != std::string::npos ||
                    line.find("AT_PARAM_ERROR") != std::string::npos || line.find("AT_BUSY_ERROR") != std::string::npos ||
                    line.find("AT_TEST_PARAM_OVERFLOW") != std::string::npos || line.find("AT_NO_CLASSB_ENABLE") != std::string::npos ||
                    line.find("AT_NO_NETWORK_JOINED") != std::string::npos || line.find("AT_RX_ERROR") != std::string::npos) 
                {
                    std::unique_lock<std::mutex> lock(_cmd_mtx);
                    _cmd_response = line;
                    _cmd_ready = true;
                    _cmd_cv.notify_one(); // Despierta a send_command de inmediato
                }
                // CASO 2: Es un downlink asíncronico de clase C
                // ejemplo: +EVT:RX_C:-12:3:UNICAST:1:22334455
                else if (line.find("+EVT:RX_C") != std::string::npos && line.find("UNICAST:") != std::string::npos) 
                {
                    SPDLOG_INFO("Receiving asynchronous message: {}", line);
                    std::vector<uint8_t> message = convertUnicastToBytes(line);
                    receive_handler(message);
                }
                // CASO 3: Es un downlink sincronico de clase A
                // ejemplo: +EVT:RX_1:-13:15:UNICAST:30:11223344
                else if ((line.find("+EVT:RX_1") != std::string::npos || line.find("+EVT:RX_2") != std::string::npos) && line.find("UNICAST:") != std::string::npos) 
                {
                    SPDLOG_INFO("Receiving synchronous message: {}", line);
                    std::vector<uint8_t> message = convertUnicastToBytes(line);
                    receive_handler(message);
                }
                else if (line.find("+EVT:TX_DONE") != std::string::npos)
                {
                    SPDLOG_DEBUG("Transmission completed: {}", line);                    
                }
                else if (line.find("AT+NJS=0") != std::string::npos)
                {
                    SPDLOG_DEBUG("Device is not joined");
                    _joined = false;

                }
                else if (line.find("AT+NJS=1") != std::string::npos)
                {
                    SPDLOG_DEBUG("Device is joined");
                    _joined = true;
                }
            }
        }
        // Un pequeño respiro si el puerto está configurado como puramente no bloqueante
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    SPDLOG_DEBUG("Background serial reader thread stopped.");
}

void SCHCLoRaWANStack::print_buffer_hex(const std::string& buffer) {
    std::cout << "--- Contenido del Buffer (Tamaño: " << buffer.size() << " bytes) ---\n";
    
    for (size_t i = 0; i < buffer.size(); ++i) {
        unsigned char c = buffer[i];
        
        // Imprimir índice de posición
        std::cout << "[" << std::setw(3) << std::setfill('0') << i << "] ";
        
        // Mostrar representación del carácter si es imprimible, o un marcador especial si es de control
        if (std::isprint(c)) {
            std::cout << "Char: '" << c << "'   ";
        } else if (c == '\r') {
            std::cout << "Char: '\\r'  ";
        } else if (c == '\n') {
            std::cout << "Char: '\\n'  ";
        } else if (c == '\0') {
            std::cout << "Char: '\\0'  ";
        } else {
            std::cout << "Char: '.'   "; // Carácter no imprimible genérico
        }
        
        // Mostrar valores numéricos en Decimal y Hexadecimal
        std::cout << " | Dec: " << std::setw(3) << std::setfill(' ') << static_cast<int>(c)
                  << " | Hex: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(c) << std::dec << "\n";
    }
    std::cout << "------------------------------------------------\n";
}