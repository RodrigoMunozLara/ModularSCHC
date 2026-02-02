#pragma once

#include <cstdint>

class ISCHCStack
{
public:
    virtual ~ISCHCStack() = default;
    virtual void send_frame(char* buff, int buff_len) = 0;
    virtual void receive_handler() = 0;
    virtual uint32_t getMtu() = 0;
};

