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
#include <condition_variable>
#include <thread>
#include <atomic>

// forward declaration
class SCHCCore;


class SCHCLoRaWANStack: public ISCHCStack
{
    public:
        SCHCLoRaWANStack(AppConfig& appConfig, SCHCCore& schcCore);
        ~SCHCLoRaWANStack() override;
        std::string send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId = std::nullopt) override;
        void        receive_handler(const std::vector<uint8_t>& frame) override;
        uint32_t    getMtu() override;
        void        init() override;
    private:
        std::string             send_command(const std::string& command, int timeoutMs=5000);
        std::string             toHexString(const std::vector<uint8_t>& data);
        std::vector<uint8_t>    convertUnicastToBytes(const std::string& input);
        void                    serial_reader_loop();
        void                    print_buffer_hex(const std::string& buffer);

        AppConfig   _appConfig;
        SCHCCore&   _schcCore;

        int         _dr;
        std::string _dev_id;

        int         _fd;
        std::string _port;
        std::mutex  _mtx; // To protect the multi-threaded port

        bool    _isFirstMsg = true;
    
    private:
        std::atomic<bool> _keep_reading{true};
        std::thread _serial_reader_thread;

        std::mutex _serial_write_mtx;         // Protege el write() si dos hilos quieren transmitir a la vez

        // Sincronización para comandos AT síncronos
        std::mutex _cmd_mtx;
        std::condition_variable _cmd_cv;
        std::string _cmd_response;            // Aquí el lector guardará la respuesta del comando AT
        bool _cmd_ready = false;              // Bandera para evitar despertares espurios

        bool _joined = true;

};