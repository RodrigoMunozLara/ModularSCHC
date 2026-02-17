#pragma once

#include <cstdint>
#include <vector>

class ISCHCState
{
    public:
        virtual ~ISCHCState() = default;
        virtual void execute(const std::vector<uint8_t>& msg = {}) = 0;
        virtual void timerExpired() = 0;
        virtual void release() = 0;
};