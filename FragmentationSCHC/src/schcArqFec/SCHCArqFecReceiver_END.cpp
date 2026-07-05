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
    SCHCGWMessage           decoder;
    SCHCMsgType             msg_type;       // message type decoded. See message type in SCHC_GW_Macros.hpp
    uint8_t                 w;              // w recibido en el mensaje
    uint8_t                 dtag = 0;       // dtag no es usado en LoRaWAN
    uint8_t                 fcn;            // fcn recibido en el mensaje
    std::vector<uint8_t>    payload;
    int                     payload_len;    // in bits

    msg_type = decoder.get_msg_type(_ctx._protoType, _ctx._ruleID, msg);


    if(!(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG))
    {
        SPDLOG_DEBUG("Receiving a late SCHC regular fragment message. Discarting Message...");
    }

    // SPDLOG_DEBUG("Setting the session as terminable");
    // _ctx._schcSession.setDead();
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
