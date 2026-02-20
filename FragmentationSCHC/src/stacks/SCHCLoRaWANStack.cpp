#include "stacks/SCHCLoRaWANStack.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "spdlog/spdlog.h"

SCHCLoRaWANStack::SCHCLoRaWANStack(AppConfig& appConfig, SCHCCore& schcCore): _appConfig(appConfig), _schcCore(schcCore)
{
    SPDLOG_DEBUG("Executing SCHCLoRaWANStack constructor()");


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
    


    std::string resp = send_command("AT+VER=?");
    SPDLOG_DEBUG("AT+VER=? {}", resp);

    // resp = sendCommand("AT+NJM=1"); /* Network Join Mode: AT+NJM: get or set the network join mode (0 = ABP, 1 = OTAA)*/
    // SPDLOG_DEBUG("{}", cleanString(resp));

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


    std::string _deveui = "AT+DEVEUI=" + _appConfig.lorawan_node.deveui;
    resp = send_command(_deveui);
    SPDLOG_DEBUG("{} {}", _deveui, resp);

    std::string _appeui = "AT+APPEUI=" + _appConfig.lorawan_node.appeui;
    resp = send_command(_appeui);
    SPDLOG_DEBUG("{} {}", _appeui, resp);

    std::string _appkey = "AT+APPKEY=" + _appConfig.lorawan_node.appkey;
    resp = send_command(_appkey);
    SPDLOG_DEBUG("{} {}", _appkey, resp);


    
    resp = send_command("AT+NJS=?");
    SPDLOG_DEBUG("{}", resp);
    size_t foundPos = resp.find("AT+NJS=0");
    if (foundPos != std::string::npos)
    {
        resp = send_command("AT+JOIN=1");  /* Join LoRaWAN Network: AT+JOIN: join network*/
        SPDLOG_DEBUG("AT+JOIN=1 {}", resp);
    }    



}

SCHCLoRaWANStack::~SCHCLoRaWANStack()
{
    SPDLOG_DEBUG("Executing SCHCLoRaWANStack destructor()");
    std::string resp = send_command("AT+JOIN=0");  /* Join LoRaWAN Network: AT+JOIN: join network*/
    SPDLOG_DEBUG("AT+JOIN=0 {}", resp);
    if (_fd != -1) close(_fd);
}

std::string SCHCLoRaWANStack::send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId)
{
    /*Creating the AT command*/
    std::stringstream ss;
    ss << "AT+SEND=" << ruleid << ":" << toHexString(buff);
    std::string command = ss.str();
    SPDLOG_DEBUG("AT command: {}", command);

    {
        std::lock_guard<std::mutex> lock(_mtx);

        /* Clean serial port */
        tcflush(_fd, TCIFLUSH);

        /* Prepare the command to write it in the serial port */
        std::string fullCmd = command + "\r\n";

        /* Write the command in the serial port */
        if (write(_fd, fullCmd.c_str(), fullCmd.length()) < 0) return "ERROR_WRITE";

        /* Initialise the variables to read messages coming from the serial port */
        std::string accumulator = "";
        auto startTime = std::chrono::steady_clock::now();
        char readBuffer[256];
        int timeoutMs = 5000;

        while (true) 
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();

            

            if (elapsed >= timeoutMs)
            {
                accumulator.erase(std::remove(accumulator.begin(), accumulator.end(), 10), accumulator.end());
                
                size_t txDonePos = accumulator.find("+EVT:TX_DONE");
                size_t rxDonePos = accumulator.find("+EVT:RX_");
                bool txDoneFlag = false;
                bool rxDoneFlag = false;
                if(txDonePos != std::string::npos) txDoneFlag = true;
                if (rxDonePos != std::string::npos) rxDoneFlag = true;
                

                if (txDoneFlag && !rxDoneFlag) 
                {
                    SPDLOG_DEBUG("AT response: {}",accumulator);
                    if(_isFirstMsg) 
                    {
                        _isFirstMsg = false;
                    }
                    return accumulator;
                }
                else if (txDoneFlag && rxDoneFlag)
                {
                    SPDLOG_DEBUG("AT response: {}",accumulator);

                    /* We extract only the fport:payload from the response. */
                    /* The _isFirstMsg flag is used for previous unwanted messages from a previous session.
                    * _isFirstMsg: true --> read the message from stack but it is not send to SCHC Core 
                    * _isFirstMsg: false --> read the message from stack and it is sent to SCHC Core
                    * */
                    if(!_isFirstMsg) 
                    {
                        std::vector<std::vector<uint8_t>> responses_vector = processModemString(accumulator);
                        for (size_t i = 0; i < responses_vector.size(); ++i)
                        {
                            receive_handler(responses_vector[i]);
                        }
            
                    }
                    else
                    {
                        _isFirstMsg = false;
                    }


                    return accumulator;
                }
            }

            /* Reding the serial port */
            int bytesRead = read(_fd, readBuffer, sizeof(readBuffer) - 1);
            if (bytesRead > 0) 
            {
                readBuffer[bytesRead] = '\0';
                accumulator += readBuffer;
            }
            // No satures el CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

  

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
        if(_dr==0 || _dr==1) return 0;
        else if(_dr==2) return 11 - fOpt;
        else if(_dr==3) return 53 - fOpt;
        else if(_dr==4) return 125 - fOpt;
        else if(_dr==4) return 242 - fOpt;
    }
    else
    {
        if(_dr==0 || _dr==1 || _dr==2) return 51 - fOpt;
        else if(_dr==3) return 115 - fOpt;
        else if(_dr==4 || _dr==5) return 222 - fOpt;
    }


    return -1;
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
}

std::string SCHCLoRaWANStack::send_command(const std::string &command, int timeoutMs)
{
std::lock_guard<std::mutex> lock(_mtx);
    
    static const std::vector<std::string> RUI3_STATUS_CODES = {
        "OK", "AT_ERROR", "AT_PARAM_ERROR", "AT_BUSY_ERROR",
        "AT_TEST_PARAM_OVERFLOW", "AT_NO_CLASSB_ENABLE",
        "AT_NO_NETWORK_JOINED", "AT_RX_ERROR"
    };

    tcflush(_fd, TCIFLUSH);
    std::string fullCmd = command + "\r\n";
    if (write(_fd, fullCmd.c_str(), fullCmd.length()) < 0) return "ERROR_WRITE";

    std::string accumulator = "";
    auto startTime = std::chrono::steady_clock::now();
    char readBuffer[256];

    while (true) 
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeoutMs) {
            // Si salimos por aquí, mostramos exactamente qué caracteres llegaron (incluyendo ocultos)
            std::string escapeDebug = accumulator;
            // Reemplazo simple para depurar caracteres de control
            return "TIMEOUT_DATA: " + accumulator;
        }

        int bytesRead = read(_fd, readBuffer, sizeof(readBuffer) - 1);
        if (bytesRead > 0) {
            readBuffer[bytesRead] = '\0';
            accumulator += readBuffer;
            accumulator.erase(std::remove(accumulator.begin(), accumulator.end(), 10), accumulator.end());

            // --- MEJORA: Búsqueda línea por línea ---
            // Buscamos cada Status Code de forma individual en el acumulador
            for (const auto& status : RUI3_STATUS_CODES) {
                size_t pos = accumulator.find(status);
                
                // Si encontramos el status (ej. "OK")
                if (pos != std::string::npos) {
                   
                    // Esperamos un micro-segundo para asegurar que el \r\n final llegó
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    
                    //SPDLOG_DEBUG("RUI3 Status Detected: {}", status);
                    return accumulator;
                }
            }
        }
        // No satures el CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

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

std::vector<uint8_t> SCHCLoRaWANStack::parseATresponse(const std::string& input) 
{
    /* From the AT response (example: +EVT:RX_1:-65:14:UNICAST:20:20), extract the fPort and payload in vector format with hexadecimal numbers. */

    std::string target = "UNICAST:";
    size_t pos              = input.find(target);
    std::string result   = input.substr(pos + target.length());

    std::vector<uint8_t> bytes;
    
    // 1. Searching the ':' separator 
    size_t colonPos = result.find(':');
    if (colonPos == std::string::npos) return bytes;

    // 2. We process the part before the “:” (always 2 digits = 1 byte). This part is the ruleID
    std::string firstPart = result.substr(0, colonPos);
    if (!firstPart.empty()) 
    {
        //bytes.push_back(static_cast<uint8_t>(std::stoul(firstPart, nullptr, 16)));
        bytes.push_back(static_cast<uint8_t>(std::stoi(firstPart)));
    }

    // 3. We process the part after the “:” (N digits = N/2 bytes). This part is the payload
    std::string secondPart = result.substr(colonPos + 1);
    
    // We reserve memory in advance to avoid reallocations.
    bytes.reserve(1 + (secondPart.length() / 2));

    for (size_t i = 0; i < secondPart.length(); i += 2) 
    {
        // We take chunks of 2 characters (e.g. ‘aa’, then ‘c3’...)
        std::string byteString = secondPart.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoul(byteString, nullptr, 16)));
    }

    return bytes;


}

// Función principal que procesa el string completo del módem
std::vector<std::vector<uint8_t>> SCHCLoRaWANStack::processModemString(const std::string& rawInput) {
    // Reutilizamos el parser que busca los bloques "UNICAST:"
    // (Asumiendo la función parseUnicastEvents que definimos antes)
    std::vector<std::string> unicastBlocks = parseUnicastEvents(rawInput);
    
    std::vector<std::vector<uint8_t>> allPackets;
    allPackets.reserve(unicastBlocks.size());

    for (const auto& block : unicastBlocks) {
        allPackets.push_back(convertUnicastToBytes(block));
    }

    return allPackets;
}

std::vector<std::string> SCHCLoRaWANStack::parseUnicastEvents(const std::string& input) {
    std::vector<std::string> results;
    std::string_view str(input);
    
    // Configuramos los nuevos "marcadores"
    const std::string_view start_tag = "UNICAST:";
    const std::string_view end_tag = "+EVT:TX";

    size_t pos = str.find(start_tag);

    while (pos != std::string_view::npos) {
        // Buscamos si hay un cierre de evento TX más adelante
        size_t next_tx = str.find(end_tag, pos + start_tag.length());
        
        std::string_view fragment;
        if (next_tx != std::string_view::npos) {
            // Cortamos desde UNICAST: hasta justo antes del +EVT:TX
            fragment = str.substr(pos, next_tx - pos);
        } else {
            // Si no hay más TX, tomamos todo hasta el final
            fragment = str.substr(pos);
        }

        results.emplace_back(fragment);

        // Buscamos el siguiente bloque UNICAST:
        pos = str.find(start_tag, pos + fragment.length());
    }

    return results;
}


// Función auxiliar basada en tu lógica para convertir UNICAST:... a bytes
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