#ifndef SCHC_Node_LoRaWAN_RAK4631_hpp
#define SCHC_Node_LoRaWAN_RAK4631_hpp

#include "SCHC_Node_Stack_L2.hpp"
#include "SCHC_Node_Macros.hpp"
#include <vector>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class SCHC_Node_Fragmenter;

class SCHC_Node_LoRaWAN_RAK4631: public SCHC_Node_Stack_L2
{
    public:
        SCHC_Node_LoRaWAN_RAK4631();
        uint8_t     initialize_stack(void);
        uint8_t     send_frame(uint8_t ruleID, char* msg, int len);
        int         getMtu(bool consider_Fopt);
        void        set_fragmenter(SCHC_Node_Fragmenter* frag);
        static SCHC_Node_Fragmenter* _frag;
    private:
        std::vector<int>    _not_send_list = {2, 3};
        int                 _not_send_countr = 1;
};


#endif