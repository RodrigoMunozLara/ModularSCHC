#include "stacks/SCHCMyriotaHTTPStack.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "spdlog/spdlog.h"

SCHCMyriotaHTTPStack::SCHCMyriotaHTTPStack(AppConfig& appConfig, SCHCCore& schcCore): _appConfig(appConfig), _schcCore(schcCore)
{
    SPDLOG_DEBUG("Executing SCHCMyriotaStack constructor()");

    this->_ngrok_port = _appConfig.myriota_http.port;
    this->_ngrok_user = _appConfig.myriota_http.ngrok_user;   

    SPDLOG_DEBUG("Instantiating WebServer on port {}", _ngrok_port);

    CROW_ROUTE(_app, "/income").methods(crow::HTTPMethod::Post)
            ([this](const crow::request& req) {
                this->onMessage(req.body);
                return crow::response(200, "OK");
            });
    
    SPDLOG_DEBUG("Starting ngrok on port {} and user {} ...", this->_ngrok_port, this->_ngrok_user);
    std::string comando = "sudo -u " + this->_ngrok_user + " ngrok http " + std::to_string(this->_ngrok_port) + " --log=stdout --log-level=info > ngrok.log 2>&1 &";
    system(comando.c_str());

}

SCHCMyriotaHTTPStack::~SCHCMyriotaHTTPStack()
{
SPDLOG_DEBUG("Executing SCHCMyriotaStack destructor()");

    _app.stop();

    std::string kill_cmd = "sudo -u " + this->_ngrok_user + " killall ngrok";
    system(kill_cmd.c_str());

    if (_server_thread.joinable()) {
        _server_thread.join();
        SPDLOG_DEBUG("Server thread joined successfully");
    }
}

std::string SCHCMyriotaHTTPStack::send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId)
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

void SCHCMyriotaHTTPStack::receive_handler(const std::vector<uint8_t>& frame)
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

uint32_t SCHCMyriotaHTTPStack::getMtu()
{
    return 99;
}

void SCHCMyriotaHTTPStack::init()
{
    SPDLOG_DEBUG("Starting the thread that will process incoming messages to the web server");
    _server_thread = std::thread([this]() {
                _app.loglevel(crow::LogLevel::Warning);
                _app.port(_ngrok_port).run();
            });
    SPDLOG_DEBUG("Thread started");
}

std::string SCHCMyriotaHTTPStack::toHexString(const std::vector<uint8_t> &data)
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

std::vector<uint8_t> SCHCMyriotaHTTPStack::parseATresponse(const std::string& input) 
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

void SCHCMyriotaHTTPStack::onMessage(const std::string& body) 
{
    //SPDLOG_DEBUG("{}", body);

    long long timestamp;
    std::string terminalId;
    std::string value;

    try {
        // Parsear el JSON principal
        json j_principal = json::parse(body);

        // Extraer el string de "Data" y parsearlo como un nuevo JSON
        // Nota: Myriota envía Data como una cadena de texto codificada
        std::string data_str = j_principal.at("Data").get<std::string>();
        json j_data = json::parse(data_str);

        // Acceder al arreglo "Packets" y extraer los valores
        // Usualmente viene al menos un paquete en el array
        if (j_data.contains("Packets") && j_data["Packets"].is_array()) 
        {
            for (const auto& packet : j_data["Packets"]) 
            {
                
                // Get information from JSON
                timestamp = packet.at("Timestamp").get<long long>();
                terminalId = packet.at("TerminalId").get<std::string>();
                value = packet.at("Value").get<std::string>();

                // Get timestamp
                SPDLOG_DEBUG("Timestamp: {}", timestamp);

                if (value.size() > 0)
                {
                    auto msgStack       = std::make_unique<StackMessage>();

                    // Get DeviceID
                    msgStack->deviceId = terminalId;
                    SPDLOG_DEBUG("DeviceID: {}", msgStack->deviceId);

                    // Get RuleID
                    std::string ruleID_str  = value.substr(0, 2);               // get two first number
                    uint8_t ruleID          = (uint8_t)std::stoi(ruleID_str);   // convert two first number to uint8_t
                    msgStack->ruleId        = ruleID;
                    value.erase(value.begin(), value.begin() + 2);              // Now data_decoded = SCHC header + SCHC payload (without RuleID)
                    SPDLOG_DEBUG("Rule ID: {}", ruleID);

                    
                    // Get payload (without RuleID)
                    //std::vector<uint8_t> data_decoded = base64_decode(value);
                    std::vector<uint8_t> data_decoded = hexToBytes(value);
                    msgStack->payload = data_decoded;


                    // Get payload length
                    msgStack->len = data_decoded.size();
                    SPDLOG_DEBUG("Decoded Payload Length: {}", msgStack->len);

                    // Print data
                    SPDLOG_DEBUG("Decoded Payload (hex format): {}", spdlog::to_hex(data_decoded));
                    
                    _schcCore.enqueueFromStack(std::move(msgStack));

                }
                else
                {
                    SPDLOG_ERROR("Payload Len is 0");
                    return;
                }
            }
        }
    } catch (json::parse_error& e) {
        SPDLOG_ERROR("Error parsing the JSON: {}", e.what());
    } catch (json::out_of_range& e) {
        SPDLOG_ERROR("Field not found in the JSON: {}", e.what());
    } catch (std::exception& e) {
        SPDLOG_ERROR("Unexpected error: {}", e.what());
    }
}

std::vector<uint8_t> SCHCMyriotaHTTPStack::base64_decode(std::string encoded)
{
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string decoded;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (base64_chars.find(c) == std::string::npos) break;
        val = (val << 6) + base64_chars.find(c);
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    std::vector<uint8_t> vec(decoded.begin(), decoded.begin() + decoded.size());
   
    return vec;
}

std::vector<uint8_t> SCHCMyriotaHTTPStack::hexToBytes(const std::string& hex) 
{
    std::vector<uint8_t> bytes;
    
    // Un hex válido debe tener un número par de caracteres
    if (hex.length() % 2 != 0) {
        throw std::runtime_error("The hexadecimal string has an odd length.");
    }

    bytes.reserve(hex.length() / 2);

    for (size_t i = 0; i < hex.length(); i += 2) {
        // Tomamos un par de caracteres: "00", "11", etc.
        std::string byteString = hex.substr(i, 2);
        
        // Convertimos la base 16 a entero
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}
