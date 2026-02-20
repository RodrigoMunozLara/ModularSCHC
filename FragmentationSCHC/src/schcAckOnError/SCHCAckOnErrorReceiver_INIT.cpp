#include "schcAckOnError/SCHCAckOnErrorReceiver_INIT.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver.hpp"

SCHCAckOnErrorReceiver_INIT::SCHCAckOnErrorReceiver_INIT(SCHCAckOnErrorReceiver &ctx): _ctx(ctx)
{

}

SCHCAckOnErrorReceiver_INIT::~SCHCAckOnErrorReceiver_INIT()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorSender_INIT destructor()");
}

void SCHCAckOnErrorReceiver_INIT::execute(const std::vector<uint8_t>& msg)
{


}

void SCHCAckOnErrorReceiver_INIT::timerExpired()
{
}

void SCHCAckOnErrorReceiver_INIT::release()
{
}
