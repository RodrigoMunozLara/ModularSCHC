#include "schcArqFec/SCHCArqFecSender.hpp"
#include "schcArqFec/SCHCArqFecSender_END.hpp"
#include <schcAckOnError/SCHCNodeMessage.hpp>
#include "SCHCSession.hpp"


SCHCArqFecSender_END::SCHCArqFecSender_END(SCHCArqFecSender& ctx): _ctx(ctx)
{
}

SCHCArqFecSender_END::~SCHCArqFecSender_END()
{
    SPDLOG_DEBUG("Executing SCHCArqFecSender_END destructor()");
}

void SCHCArqFecSender_END::execute(const std::vector<uint8_t>& msg)
{
    SPDLOG_DEBUG("Setting the session as terminable");
    _ctx._schcSession.setDead();
    return;
}

void SCHCArqFecSender_END::timerExpired()
{
    SPDLOG_DEBUG("Setting the session as terminable");
    _ctx._schcSession.setDead();
    return;
}

void SCHCArqFecSender_END::release()
{
}
