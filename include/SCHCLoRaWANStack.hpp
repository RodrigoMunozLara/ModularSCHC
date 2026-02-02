#pragma once

#include "ISCHCStack.hpp"
#include "ConfigStructs.hpp"
#include <cstdint>

// forward declaration
class SCHCCore;


class SCHCLoRaWANStack: public ISCHCStack
{
    public:
        SCHCLoRaWANStack(AppConfig& appConfig, SCHCCore& schcCore);
        ~SCHCLoRaWANStack() override;
        void send_frame(char* buff, int buff_len) override;
        void receive_handler() override;
        uint32_t getMtu() override;
    private:
        AppConfig   _appConfig;
        SCHCCore&   _schcCore;
};