#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include <unordered_map>
#include "SCHC_Compressor.hpp"
#include "SCHC_Rule.hpp"
#include "SCHC_RulesManager.hpp"
#include "PacketParser.hpp"
#include "SCHC_Packet.hpp"


static STATE_RESULT st_idle(FSM_Ctx& ctx);
static STATE_RESULT st_parse(FSM_Ctx& ctx);
static STATE_RESULT st_find_rule(FSM_Ctx& ctx);
static STATE_RESULT st_mo(FSM_Ctx& ctx);
static STATE_RESULT st_cda(FSM_Ctx& ctx);
static STATE_RESULT st_gen(FSM_Ctx& ctx);
static STATE_RESULT st_decomp(FSM_Ctx& ctx);

// ---- ctor: asigna handlers ----
CompressorFSM::CompressorFSM() {
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_IDLE)]      = st_idle;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_PARSE)]     = st_parse;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_FIND_RULE)] = st_find_rule;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_MO)]        = st_mo;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_CDA)]       = st_cda;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_GEN)]       = st_gen;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_DECOMP)]    = st_decomp;
}

STATE_RESULT CompressorFSM::stepFSM(FSM_Ctx& ctx) {
    auto i = static_cast<std::size_t>(state_);
    StateFn fn = handlers_[i];
    if (!fn) {
        ctx.error_code = 1;
        return STATE_RESULT::ERROR;
    }

    ctx.next_state = state_;          // default: quedarse
    STATE_RESULT r = fn(ctx);         // ejecutar 1 vez

    if (r == STATE_RESULT::PASS) {
        state_ = ctx.next_state;      // transicionar
    }
    return r;
}

STATE_RESULT CompressorFSM::runFSM(FSM_Ctx& ctx, uint32_t max_steps) {
    // arranca en PARSE típicamente
    state_ = COMP_STATE::COMP_STATE_PARSE;

    for (uint32_t k = 0; k < max_steps; ++k) {
        STATE_RESULT r = stepFSM(ctx);
        if (r == STATE_RESULT::STOP || r == STATE_RESULT::ERROR || r == STATE_RESULT::FAIL) {
            return r;
        }
    }
    ctx.error_code = 0xFE; // loop guard
    return STATE_RESULT::ERROR;
}


//----------------  State Functions ---------------------


//IDLE STATE: Waiting to change when a package arrives
//FOR NOW IT ASUMES IT IS A NON SCHC PACKET, JUSTP IPV6-UDP
static STATE_RESULT st_idle(FSM_Ctx& ctx) {
    if(ctx.arrived){
        ctx.next_state = COMP_STATE::COMP_STATE_PARSE;
        return STATE_RESULT::PASS;
    }
    else{
        return STATE_RESULT::STAY;
    }
}

static STATE_RESULT st_parse(FSM_Ctx& ctx) {
    if (!ctx.raw_pkt) { ctx.error_code = 2; return STATE_RESULT::ERROR; }

    bool link = (ctx.direction == direction_indicator_t::DOWN);

    if(link){//If "DOWN" direction (to the device), parse for compression
            
        // parse IPv6+UDP -> vector<FieldValue>
        ctx.parsedPacket = parse_ipv6_udp_fields(*ctx.raw_pkt, link);

        // build idx
        ctx.idx.clear();
        ctx.idx.reserve(ctx.parsedPacket.size());
        for (size_t i = 0; i < ctx.parsedPacket.size(); ++i) {
            ctx.idx.emplace(ctx.parsedPacket[i].fid, i);
        }

        ctx.next_state = COMP_STATE::COMP_STATE_FIND_RULE;
    }/*else{ //if "UP" direction, parse for decompression. Not implemented yet.
        ctx.next_state = COMP_STATE::COMP_STATE_DECOMP;
    }*/

    if (ctx.parsedPacket.empty()) return STATE_RESULT::FAIL;

    
    return STATE_RESULT::PASS;
}

static STATE_RESULT st_find_rule(FSM_Ctx& ctx) {
    if (!ctx.rule_ctx) { ctx.error_code = 3; return STATE_RESULT::ERROR; }

    ctx.selected_rule = nullptr;

    for (const auto& rule : *ctx.rule_ctx) {
        if (rule.getNatureType() != nature_type_t::COMPRESSION) continue; //skips rules for fragmentation and no compression
        if (ctx.parsedPacket.size()  != rule.getFields().size()) continue; //skips rules if the amount of FIDs aren't equal

        
        

    }

    if (!ctx.selected_rule) return STATE_RESULT::FAIL;

    ctx.next_state = COMP_STATE::COMP_STATE_MO;
    return STATE_RESULT::PASS;
}




