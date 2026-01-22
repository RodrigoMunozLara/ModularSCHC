#ifndef SCHC_Node_Stack_L2_hpp
#define SCHC_Node_Stack_L2_hpp

#include <cstdint>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class SCHC_Node_Stack_L2
{
public:
    virtual uint8_t initialize_stack(void) = 0;
    virtual uint8_t send_frame(uint8_t ruleID, char* msg, int len) = 0;
    virtual int getMtu(bool consider_Fopt) = 0;
};

#endif