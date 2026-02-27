#pragma once

#include "interfaces/ISCHCState.hpp"
#include "interfaces/ISCHCStack.hpp"
#include <spdlog/spdlog.h>
#include "spdlog/fmt/bin_to_hex.h"
#include <memory>


class SCHCArqFecSender;   // ← forward declaration

class SCHCArqFecSender_END: public ISCHCState
{
    public:
        SCHCArqFecSender_END(SCHCArqFecSender& ctx);
        ~SCHCArqFecSender_END();
        void execute(const std::vector<uint8_t>& msg = {}) override;
        void timerExpired() override;
        void release() override;
    private:

        SCHCArqFecSender& _ctx;
};
