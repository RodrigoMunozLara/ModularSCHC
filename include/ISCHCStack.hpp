#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <string>

class ISCHCStack
{
public:
    virtual ~ISCHCStack() = default;
    virtual std::string send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId = std::nullopt) = 0;
    virtual void receive_handler(const std::vector<uint8_t>& frame) = 0;
    virtual uint32_t getMtu() = 0;
    virtual void    init() = 0;
};

