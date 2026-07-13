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
    if(!msg.empty())
    {
        SCHCGWMessage           decoder;
        SCHCMsgType             msg_type;       // message type decoded. See message type in SCHC_GW_Macros.hpp
        uint8_t                 w;              // w recibido en el mensaje
        uint8_t                 dtag = 0;       // dtag no es usado en LoRaWAN
        uint8_t                 fcn;            // fcn recibido en el mensaje
        std::vector<uint8_t>    payload;
        int                     payload_len;    // in bits

        msg_type = decoder.get_msg_type(_ctx._protoType, _ctx._ruleID, msg);


        if(msg_type == SCHCMsgType::SCHC_REGULAR_FRAGMENT_MSG)
        {
            SPDLOG_DEBUG("Receiving a late SCHC regular fragment message. Discarting Message...");
        }
        else if(msg_type == SCHCMsgType::SCHC_ALL1_FRAGMENT_MSG)
        {
            SPDLOG_INFO("|--- W={:<1}, FCN={:<2}+RCS ->| {:>2} bits", w, fcn, _ctx._lastTileSize*8);

            /* Enviando ACK para confirmar la sesion */
            SPDLOG_DEBUG("Sending SCHC ACK");
            SCHCGWMessage    encoder;
            uint8_t c                   = 1;
            uint8_t w                   = 3;
            std::vector<uint8_t> buffer = encoder.create_schc_ack(_ctx._ruleID, dtag, w, c);

            _ctx._stack->send_frame(static_cast<int>(SCHCLoRaWANFragRule::SCHC_FRAG_UPDIR_RULE_ID), buffer, _ctx._dev_id);

            SPDLOG_INFO("|<-- ACK, W={:<1}, C={:<1} --|", w, c);        
        }

        // SPDLOG_DEBUG("Setting the session as terminable");
        // _ctx._schcSession.setDead();
        return;
    }
    else
    {
        SPDLOG_WARN("It is not permitted to call the execute method without a message");
        _ctx.executeTimer(_ctx._inactivityTimer);
        return;
    }
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
