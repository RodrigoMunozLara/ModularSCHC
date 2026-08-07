#include "stacks/SCHCMyriotaStack.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "spdlog/spdlog.h"

SCHCMyriotaStack::SCHCMyriotaStack(AppConfig& appConfig, SCHCCore& schcCore): _appConfig(appConfig), _schcCore(schcCore)
{
    SPDLOG_DEBUG("Executing SCHCMyriotaStack constructor()");

    _keep_reading = true;

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

    // Baud rate set to 9600 (modem standard)
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

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
    tty.c_cc[VTIME] = 20;   // 2 seconds timeout for each read

    if (tcsetattr(_fd, TCSANOW, &tty) != 0) {
        close(_fd);
        SPDLOG_ERROR("Error configuring the port. Closing File descriptor");
    }
    SPDLOG_DEBUG("Successfully connected to {} port", _port);

    /* ***** Configuring Myriota modem ***** */
    
    // We start the reader thread at the end of the constructor, once the port has been configured
    _serial_reader_thread = std::thread(&SCHCMyriotaStack::serial_reader_loop, this);
    SPDLOG_DEBUG("Waiting 2000 ms.....");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::string resp = send_command("AT+STATE=?");  /* System state (INITIALIZING, GNSS_ACQ or READY) 	Refer to power up logic */
    SPDLOG_DEBUG("AT+STATE=? --> {}", resp);    

    resp = send_command("AT+SUSPEND=?");            /* Get suspend mode (0: disabled, 1:enabled) */
    SPDLOG_DEBUG("AT+SUSPEND=? --> {}", resp);

    resp = send_command("AT+VSDK=?");               /* SDK version (SDK version: Format: major.minor.patch) */
    SPDLOG_DEBUG("AT+VSDK=? --> {}", resp);

    resp = send_command("AT+MID=?");                /* Module ID (Module ID and part number, E.g. 0012345678 M1-24)*/
    SPDLOG_DEBUG("AT+MID=? --> {}", resp);

    resp = send_command("AT+REGCODE=?");            /* Registration code*/
    SPDLOG_DEBUG("AT+REGCODE=? --> {}", resp);

    resp = send_command("AT+TIME=?");               /* Get time (Unix epoch time) */
    SPDLOG_DEBUG("AT+TIME=? --> {}", resp);

    resp = send_command("AT+LOCATION=?");           /* Get location (Latitude and longitude of last GNSS fix, scaled by 1e7, E.g. -349205499,1386086737)*/
    SPDLOG_DEBUG("AT+LOCATION=? --> {}", resp);

    resp = send_command("AT+MSGQ=?");               /* Message queue 	MSGQ 	Number of free slots in the message queue*/
    SPDLOG_DEBUG("AT+MSGQ=? --> {}", resp);

    resp = send_command("AT+MSGQS=?");              /* Message queue status	(Transmission status of messages in the message queue)*/
    SPDLOG_DEBUG("AT+MSGQS=? --> {}", resp);

}

SCHCMyriotaStack::~SCHCMyriotaStack()
{
SPDLOG_DEBUG("Executing SCHCMyriotaStack destructor()");
    
    // First, we notify the thread that it needs to end
    _keep_reading = false;
    
    // We wait patiently for the thread to run out
    if (_serial_reader_thread.joinable()) {
        _serial_reader_thread.join();
    }

    // With no active secondary threads remaining, we send the exit command and close the process   
    if (_fd != -1) {
        close(_fd);
        _fd = -1;
    }
}

void SCHCMyriotaStack::init()
{
    SPDLOG_DEBUG("Starting SCHCMyriotaStack");

}

std::string SCHCMyriotaStack::send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId)
{
/* Creating the AT command */
    std::stringstream ss;
    std::vector<uint8_t> ruleid_vec = { static_cast<uint8_t>(ruleid) };
    ss << "AT+SMSG=" << toHexString(ruleid_vec) << toHexString(buff);
    std::string command = ss.str();
    SPDLOG_DEBUG("{}", command);

    int timeoutMs = 3000;
    std::string res = send_command(command);
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));

    return res;
}

std::string SCHCMyriotaStack::send_command(const std::string &command, int timeoutMs)
{
    // 1. Bloqueamos cmdLock desde el inicio. Nadie podrá modificar _cmd_ready o 
    // procesar respuestas hasta que entremos a wait_for().
    std::unique_lock<std::mutex> cmdLock(_cmd_mtx);
    _cmd_ready = false;
    _cmd_response.clear();

    {
        // 2. Protegemos la escritura en el puerto serie
        std::unique_lock<std::mutex> writeLock(_serial_write_mtx);

        std::string fullCmd = command + "\r\n";
        // SPDLOG_DEBUG("Writing AT Command: {}", command);
        
        if (write(_fd, fullCmd.c_str(), fullCmd.length()) < 0) {
            // En caso de error, aseguramos dejar el estado limpio antes de salir
            _cmd_ready = false; 
            return "ERROR_WRITE";
        }
        
        // writeLock se libera automáticamente aquí al salir del scope {}
    }

    // 3. Pasamos cmdLock DIRECTAMENTE al wait_for.
    // Al entrar a wait_for, cmdLock se libera de forma ATÓMICA.
    // Si Myriota ya había respondido, el predicado [_cmd_ready] se evalúa 
    // de inmediato y la función continúa sin bloquearse.
    bool success = _cmd_cv.wait_for(cmdLock, std::chrono::milliseconds(timeoutMs), [this]() {
        return _cmd_ready;
    });

    if (!success) {
        SPDLOG_WARN("Timeout waiting for command response: {}", command);
        return "TIMEOUT_DATA";
    }

    return _cmd_response;

}

void SCHCMyriotaStack::receive_handler(const std::vector<uint8_t>& frame)
{
    /* ToDo */
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
    /* The data is packed into 20-byte Myriota Messages and scheduled via the Message Management for transmission 
    *
    * https://support.myriota.com/hc/en-us/articles/6533192310031-Network-Overview
    * */

    /* I reserve one byte for the RuleID because the state machine does not send the 
    * RuleID (inherited from LoRaWAN, which sends the RuleID in the fPort field) 
    */
    return 19; 
}

std::string SCHCMyriotaStack::toHexString(const std::vector<uint8_t> &data)
{
    std::stringstream ss;
    // We set up the stream just once
    ss << std::hex << std::setfill('0') << std::uppercase;
    
    // We use a range-based for loop, which is cleaner and avoids indexing errors
    for (const auto& byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    
    return ss.str();
}

void SCHCMyriotaStack::serial_reader_loop()
{
    SPDLOG_DEBUG("Starting background thread to read serial port");
    std::string accumulator = "";
    char readBuffer[256];

    while (_keep_reading) 
    {
        // A read operation that blocks or has a short timeout (VTIME=1), with no mutex to halt it
        int bytesRead = read(_fd, readBuffer, sizeof(readBuffer) - 1);
        if (bytesRead > 0) 
        {
            readBuffer[bytesRead] = '\0';
            accumulator += readBuffer;
            //print_buffer_hex(readBuffer);

            // https://github.com/Myriota/SDK/blob/master/examples/at_modem/README.md
            // * Commands start with prefix AT
            // * Commands are case sensitive. All characters in commands should be in upper case
            // * All white spaces such as <CR><LF>, <LF> and Space in a command are recognized as command terminators
            // * Responses end with <CR><LF>
            // * <CR> stands for carriage return character. ASCII value is 13
            // * <LF> stands for line feed character. ASCII value is 10
            // * Maximum command length is 80 characters including prefix and terminators
            // * Commands have 2 types - query and control
            // * All commands have a response 
            size_t pos;
            while ((pos = accumulator.find("\n\n")) != std::string::npos) 
            {
                std::string line = accumulator.substr(0, pos);
                accumulator.erase(0, pos + 2);

                if (line.empty()) continue; // Responses end with <CR><LF>

                // Caso 1: Se recibe algun status 
                if (line.find("OK+") != std::string::npos || line.find("FAIL+") != std::string::npos ||
                    line.find("INVALID_PARAMETER") != std::string::npos || line.find("MESSAGE_TOO_LONG") != std::string::npos ||
                    line.find("TOO_MANY_MESSAGES") != std::string::npos || line.find("BUFFER_OVERFLOW") != std::string::npos ||
                    line.find("UNKNOWN_QUERY_CMD") != std::string::npos || line.find("UNKNOWN_CONTROL_CMD") != std::string::npos ||
                    line.find("INVALID_COMMAND") != std::string::npos || line.find("") != std::string::npos) 
                {

                    // INVALID_PARAMETER 	Control commands are carrying illegal parameters 	Debug port(UART0 - 115200,N,8,1) can be used to monitor detail causes when debugging
                    // MESSAGE_TOO_LONG 	Scheduled message is too long 	Reduce message size
                    // TOO_MANY_MESSAGES 	Too many messages scheduled in an hour 	Wait for sometime before scheduling the message
                    // BUFFER_OVERFLOW 	Modem RX buffer overflow 	Reduce UART frame length
                    // UNKNOWN_QUERY_CMD 	Query identifier "=?" detected but no command match 	Check query command list
                    // UNKNOWN_CONTROL_CMD 	Control format matched but no command is found 	Check control command list
                    // INVALID_COMMAND 	Command format error, can be caused by prefix/terminator match or command/parameter too long 	Check command format or monitor debug port for detail reason                    
                    std::unique_lock<std::mutex> lock(_cmd_mtx);
                    _cmd_response = line;
                    _cmd_ready = true;
                    _cmd_cv.notify_one(); // Despierta a send_command de inmediato
                }
            }
        }
        // A slight delay if the port is configured as purely non-blocking
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    SPDLOG_DEBUG("Background serial reader thread stopped.");
}

void SCHCMyriotaStack::print_buffer_hex(const std::string& buffer) {
    std::cout << "--- Buffer contents (Size: " << buffer.size() << " bytes) ---\n";
    
    for (size_t i = 0; i < buffer.size(); ++i) {
        unsigned char c = buffer[i];
        
        // Print position index
        std::cout << "[" << std::setw(3) << std::setfill('0') << i << "] ";
        
        // Display the character if it is printable, or a special marker if it is a control character
        if (std::isprint(c)) {
            std::cout << "Char: '" << c << "'   ";
        } else if (c == '\r') {
            std::cout << "Char: '\\r'  ";
        } else if (c == '\n') {
            std::cout << "Char: '\\n'  ";
        } else if (c == '\0') {
            std::cout << "Char: '\\0'  ";
        } else {
            std::cout << "Char: '.'   "; // Generic non-printable character
        }
        
        // Display numerical values in decimal and hexadecimal
        std::cout << " | Dec: " << std::setw(3) << std::setfill(' ') << static_cast<int>(c)
                  << " | Hex: 0x" << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(c) << std::dec << "\n";
    }
    std::cout << "------------------------------------------------\n";
}