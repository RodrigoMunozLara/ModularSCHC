#include "SCHCAckOnErrorSender_ERROR.hpp"
#include "SCHCAckOnErrorSender.hpp"
#include "SCHCSession.hpp"

SCHCAckOnErrorSender_ERROR::SCHCAckOnErrorSender_ERROR(SCHCAckOnErrorSender& ctx): _ctx(ctx)
{
}

SCHCAckOnErrorSender_ERROR::~SCHCAckOnErrorSender_ERROR()
{
    SPDLOG_DEBUG("Executing SCHCAckOnErrorSender_ERROR destructor()");
}

void SCHCAckOnErrorSender_ERROR::execute(const std::vector<uint8_t>& msg)
{
    _ctx._schcSession.setDead();
    return;
}

void SCHCAckOnErrorSender_ERROR::timerExpired()
{
}

void SCHCAckOnErrorSender_ERROR::release()
{
}
