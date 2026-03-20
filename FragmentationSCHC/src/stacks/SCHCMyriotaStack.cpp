#include "stacks/SCHCMyriotaStack.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "spdlog/spdlog.h"

SCHCMyriotaStack::SCHCMyriotaStack(AppConfig& appConfig, SCHCCore& schcCore): _appConfig(appConfig), _schcCore(schcCore)
{
    SPDLOG_DEBUG("Executing SCHCMyriotaStack constructor()");


    /* ***** Configuring serial connection **** */
    std::string _port = _appConfig.myriota_node.serial_port;
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




    /*Creating the AT command*/
    std::stringstream ss;
    ss << "AT+STATUS";
    std::string command = ss.str();
    SPDLOG_DEBUG("AT command: {}", command);

    if (_fd < 0) {
        SPDLOG_ERROR("Attempting to write to a port that was not opened correctly (fd={})", _fd);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_mtx);

        /* Clean serial port */
        tcflush(_fd, TCIFLUSH);

        /* Prepare the command to write it in the serial port */
        std::string fullCmd = command + "\r\n";

        /* Write the command in the serial port */
        if (write(_fd, fullCmd.c_str(), fullCmd.length()) < 0)
        {
            SPDLOG_ERROR("Writing to the serial port is causing errors");
            return;
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(2000));


        /* Initialise the variables to read messages coming from the serial port */
        std::string accumulator = "";
        auto startTime = std::chrono::steady_clock::now();
        char readBuffer[256];
        int timeoutMs = 10000;

        while (true) 
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();

        
            if (elapsed >= timeoutMs)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                accumulator.erase(std::remove(accumulator.begin(), accumulator.end(), 10), accumulator.end());
                SPDLOG_DEBUG("accumulator: {}", accumulator);
                size_t txDonePos = accumulator.find("+EVT:STATUS");
                bool txDoneFlag = false;
                if(txDonePos != std::string::npos) txDoneFlag = true;
                
                if (txDoneFlag) 
                {
                    SPDLOG_DEBUG("AT response: {}",accumulator);
                    if(_isFirstMsg) 
                    {
                        _isFirstMsg = false;
                    }
                    return;
                }
                else
                {
                    SPDLOG_DEBUG("Nothing has been received");
                    return;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }


    }











}

SCHCMyriotaStack::~SCHCMyriotaStack()
{
    SPDLOG_DEBUG("Executing SCHCMyriotaStack destructor()");

    if (_fd != -1) close(_fd);
}

std::string SCHCMyriotaStack::send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId)
{
    /*Creating the AT command*/
    std::stringstream ss;
    ss << "AT+SEND=" << ruleid << ":" << toHexString(buff);
    std::string command = ss.str();
    SPDLOG_DEBUG("AT command: {}", command);

    if (_fd < 0) {
        SPDLOG_ERROR("Attempting to write to a port that was not opened correctly (fd={})", _fd);
        return "ERROR_INVALID_FD";
    }

    {
        std::lock_guard<std::mutex> lock(_mtx);

        /* Clean serial port */
        tcflush(_fd, TCIFLUSH);

        /* Prepare the command to write it in the serial port */
        std::string fullCmd = command + "\r\n";

        /* Write the command in the serial port */
        if (write(_fd, fullCmd.c_str(), fullCmd.length()) < 0)
        {
            SPDLOG_ERROR("Writing to the serial port is causing errors");
            return "ERROR_WRITE";
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(2000));


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
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                accumulator.erase(std::remove(accumulator.begin(), accumulator.end(), 10), accumulator.end());
                
                size_t txDonePos = accumulator.find("+EVT:TX_");
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
                    // std::vector<std::vector<uint8_t>> responses_vector = processModemString(accumulator);
                    // for (size_t i = 0; i < responses_vector.size(); ++i)
                    // {
                    //     receive_handler(responses_vector[i]);
                    // }
            


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
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }


    }
    
}

void SCHCMyriotaStack::receive_handler(const std::vector<uint8_t>& frame)
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

uint32_t SCHCMyriotaStack::getMtu()
{
    return 99;
}

void SCHCMyriotaStack::init()
{

}

std::string SCHCMyriotaStack::toHexString(const std::vector<uint8_t> &data)
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

std::vector<uint8_t> SCHCMyriotaStack::parseATresponse(const std::string& input) 
{
    // /* From the AT response (example: +EVT:RX_1:-65:14:UNICAST:20:20), extract the fPort and payload in vector format with hexadecimal numbers. */

    // std::string target = "UNICAST:";
    // size_t pos              = input.find(target);
    // std::string result   = input.substr(pos + target.length());

    std::vector<uint8_t> bytes;
    
    // // 1. Searching the ':' separator 
    // size_t colonPos = result.find(':');
    // if (colonPos == std::string::npos) return bytes;

    // // 2. We process the part before the “:” (always 2 digits = 1 byte). This part is the ruleID
    // std::string firstPart = result.substr(0, colonPos);
    // if (!firstPart.empty()) 
    // {
    //     //bytes.push_back(static_cast<uint8_t>(std::stoul(firstPart, nullptr, 16)));
    //     bytes.push_back(static_cast<uint8_t>(std::stoi(firstPart)));
    // }

    // // 3. We process the part after the “:” (N digits = N/2 bytes). This part is the payload
    // std::string secondPart = result.substr(colonPos + 1);
    
    // // We reserve memory in advance to avoid reallocations.
    // bytes.reserve(1 + (secondPart.length() / 2));

    // for (size_t i = 0; i < secondPart.length(); i += 2) 
    // {
    //     // We take chunks of 2 characters (e.g. ‘aa’, then ‘c3’...)
    //     std::string byteString = secondPart.substr(i, 2);
    //     bytes.push_back(static_cast<uint8_t>(std::stoul(byteString, nullptr, 16)));
    // }

    return bytes;


}

