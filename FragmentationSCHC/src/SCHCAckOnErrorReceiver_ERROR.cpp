#include "SCHCAckOnErrorReceiver_ERROR.hpp"
#include "SCHCAckOnErrorReceiver.hpp"

SCHCAckOnErrorReceiver_ERROR::SCHCAckOnErrorReceiver_ERROR(SCHCAckOnErrorReceiver & ctx): _ctx(ctx)
{
}

SCHCAckOnErrorReceiver_ERROR::~SCHCAckOnErrorReceiver_ERROR()
{
}

void SCHCAckOnErrorReceiver_ERROR::execute(const std::vector<uint8_t>& msg)
{
}

void SCHCAckOnErrorReceiver_ERROR::timerExpired()
{
}

void SCHCAckOnErrorReceiver_ERROR::release()
{
}
