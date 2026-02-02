#include "SCHCLoRaWANStack.hpp"
#include "SCHCCore.hpp"
#include "ConfigStructs.hpp"
#include "spdlog/spdlog.h"

SCHCLoRaWANStack::SCHCLoRaWANStack(AppConfig& appConfig, SCHCCore& schcCore): _appConfig(appConfig), _schcCore(schcCore)
{
    SPDLOG_INFO("Executing SCHCLoRaWANStack constructor()");
}

SCHCLoRaWANStack::~SCHCLoRaWANStack()
{
    SPDLOG_INFO("Executing SCHCLoRaWANStack destructor()");
}

void SCHCLoRaWANStack::send_frame(char *buff, int buff_len)
{
}

void SCHCLoRaWANStack::receive_handler()
{
}

uint32_t SCHCLoRaWANStack::getMtu()
{
    return 0;
}
