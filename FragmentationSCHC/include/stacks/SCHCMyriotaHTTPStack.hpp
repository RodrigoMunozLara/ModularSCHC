#pragma once

#include "interfaces/ISCHCStack.hpp"
#include "ConfigStructs.hpp"
#include <cstdint>

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <unordered_set>
#include "crow/crow_all.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// forward declaration
class SCHCCore;


class SCHCMyriotaHTTPStack: public ISCHCStack
{
    public:
        SCHCMyriotaHTTPStack(AppConfig& appConfig, SCHCCore& schcCore);
        ~SCHCMyriotaHTTPStack() override;
        std::string send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId = std::nullopt) override;
        void        receive_handler(const std::vector<uint8_t>& frame) override;
        uint32_t    getMtu() override;
        void        init() override;
    private:
        std::string             toHexString(const std::vector<uint8_t>& data);
        std::vector<uint8_t>    parseATresponse(const std::string& input);
        void                    onMessage(const std::string& body);
        std::vector<uint8_t>    base64_decode(std::string encoded);
        std::vector<uint8_t>    hexToBytes(const std::string& hex);


        AppConfig   _appConfig;
        SCHCCore&   _schcCore;

        int         _dr;
        std::string _dev_id;

        int         _fd;
        std::string _port;
        std::mutex  _mtx; // To protect the multi-threaded port

        bool    _isFirstMsg = true;

        crow::SimpleApp _app;
        std::thread     _server_thread;
        int             _ngrok_port;
        std::string     _ngrok_user;

};