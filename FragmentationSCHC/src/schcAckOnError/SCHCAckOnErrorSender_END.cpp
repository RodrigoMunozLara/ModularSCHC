#include "schcAckOnError/SCHCAckOnErrorSender_END.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender.hpp"
#include "SCHCSession.hpp"

SCHCAckOnErrorSender_END::SCHCAckOnErrorSender_END(SCHCAckOnErrorSender& ctx): _ctx(ctx)
{
}

SCHCAckOnErrorSender_END::~SCHCAckOnErrorSender_END()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorSender_END destructor()");
}

void SCHCAckOnErrorSender_END::execute(const std::vector<uint8_t>& msg)
{
    SPDLOG_DEBUG("Setting the session as terminable");
    _ctx._schcSession.setDead();
    return;
}

void SCHCAckOnErrorSender_END::timerExpired()
{
    SPDLOG_DEBUG("Setting the session as terminable");
    _ctx._schcSession.setDead();
    return;
}

void SCHCAckOnErrorSender_END::release()
{
}
