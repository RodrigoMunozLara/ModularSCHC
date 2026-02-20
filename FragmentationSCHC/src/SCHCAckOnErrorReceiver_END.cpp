#include "SCHCAckOnErrorReceiver_END.hpp"
#include "SCHCAckOnErrorReceiver.hpp"
#include "SCHCSession.hpp"

SCHCAckOnErrorReceiver_END::SCHCAckOnErrorReceiver_END(SCHCAckOnErrorReceiver &ctx): _ctx(ctx)
{
}

SCHCAckOnErrorReceiver_END::~SCHCAckOnErrorReceiver_END()
{
}

void SCHCAckOnErrorReceiver_END::execute(const std::vector<uint8_t>& msg)
{
    SPDLOG_DEBUG("Setting the session as terminable");
    _ctx._schcSession.setDead();
    return;
}

void SCHCAckOnErrorReceiver_END::timerExpired()
{
    SPDLOG_DEBUG("Setting the session as terminable");
    _ctx._schcSession.setDead();
    return;
}

void SCHCAckOnErrorReceiver_END::release()
{
}
