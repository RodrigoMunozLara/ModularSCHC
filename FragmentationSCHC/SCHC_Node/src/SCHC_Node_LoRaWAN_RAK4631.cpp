#include "SCHC_Node_LoRaWAN_RAK4631.hpp"
#include "SCHC_Node_Fragmenter.hpp"

SCHC_Node_Fragmenter* SCHC_Node_LoRaWAN_RAK4631::_frag = nullptr;

SCHC_Node_LoRaWAN_RAK4631::SCHC_Node_LoRaWAN_RAK4631()
{

}

uint8_t SCHC_Node_LoRaWAN_RAK4631::initialize_stack(void)
{


    return 0;
}

uint8_t SCHC_Node_LoRaWAN_RAK4631::send_frame(uint8_t ruleID, char* msg, int len)
{

    SPDLOG_TRACE("Entering the function");


    SPDLOG_TRACE("Leaving the function");
    return 0;
}

int SCHC_Node_LoRaWAN_RAK4631::getMtu(bool consider_Fopt)
{
    return 0;
}

void SCHC_Node_LoRaWAN_RAK4631::set_fragmenter(SCHC_Node_Fragmenter *frag)
{
    SCHC_Node_LoRaWAN_RAK4631::_frag = frag;
}




