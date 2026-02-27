#include "schcArqFec/SCHCArqFecReceiver_END.hpp"
#include "schcArqFec/SCHCArqFecReceiver.hpp"
#include "SCHCSession.hpp"


SCHCArqFecReceiver_END::SCHCArqFecReceiver_END(SCHCArqFecReceiver &ctx): _ctx(ctx)
{
}

SCHCArqFecReceiver_END::~SCHCArqFecReceiver_END()
{
    SPDLOG_DEBUG("Executing SCHCArqFecReceiver_END destructor()");
}

void SCHCArqFecReceiver_END::execute(const std::vector<uint8_t>& msg)
{
    SPDLOG_DEBUG("Setting the session as terminable");
    _ctx._schcSession.setDead();
    return;
}

void SCHCArqFecReceiver_END::timerExpired()
{
    SPDLOG_DEBUG("Setting the session as terminable");
    _ctx._schcSession.setDead();
    return;
}

void SCHCArqFecReceiver_END::release()
{
}
