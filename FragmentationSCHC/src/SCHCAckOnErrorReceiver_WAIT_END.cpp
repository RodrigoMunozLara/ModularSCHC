#include "SCHCAckOnErrorReceiver_WAIT_END.hpp"
#include "SCHCAckOnErrorReceiver.hpp"

SCHCAckOnErrorReceiver_WAIT_END::SCHCAckOnErrorReceiver_WAIT_END(SCHCAckOnErrorReceiver &ctx): _ctx(ctx)
{
}

SCHCAckOnErrorReceiver_WAIT_END::~SCHCAckOnErrorReceiver_WAIT_END()
{
}

void SCHCAckOnErrorReceiver_WAIT_END::execute(const std::vector<uint8_t>& msg)
{
}

void SCHCAckOnErrorReceiver_WAIT_END::timerExpired()
{
}

void SCHCAckOnErrorReceiver_WAIT_END::release()
{
}
