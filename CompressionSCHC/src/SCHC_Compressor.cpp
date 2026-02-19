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

//------------ Auxiliar
uint16_t value_size_in_bits(uint16_t n) { //To calculate how many bits needed to represent the number
    uint16_t bits = 0;
    while (n > 0) {
        n >>= 1;
        bits++;
    }
    return bits == 0 ? 1 : bits;
}

bool msb_comparator(const std::vector<uint8_t> &vectorA,const std::vector<uint8_t> &vectorB, size_t msb){
     const size_t nbytes = msb / 8;
    const size_t rembits = msb % 8;

    if (vectorA.size() < nbytes + (rembits ? 1 : 0)) return false;
    if (vectorB.size() < nbytes + (rembits ? 1 : 0)) return false;

    for(size_t i = 0; i<nbytes ; i++){
        if(vectorA[i]!=vectorB[i]){
            return false;
        }
    }
    if(rembits){
        uint8_t mask = static_cast<uint8_t>(0xFF << (8-rembits));
        if ( (vectorA[nbytes] & mask) != (vectorB[nbytes] & mask) ) return false;

    }
    return true;
}

uint16_t lsb_extractor(const std::vector<uint8_t> value, uint16_t bits){    
    //Working only for fixed FL
    if (bits == 0) return 0; //error in extracting

    if (bits > 16) bits = 16;

    uint64_t out = 0;

    size_t bits_left = bits;

    for (size_t pos = 0; pos < value.size() && bits_left > 0; ++pos)
    {
        uint8_t b = value[value.size() - 1 - pos];

        size_t take = (bits_left >= 8) ? 8 : bits_left;

        uint8_t part = static_cast<uint8_t>(b & ((1u << take) - 1u));
        out |= (static_cast<uint64_t>(part) << (bits - bits_left));

        bits_left -= take;
    }
    return out;
}

// --------------------------------------------//
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

    state_ = COMP_STATE::COMP_STATE_IDLE;

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
    if (!ctx.rulesCtx) { ctx.error_code = 3; return STATE_RESULT::ERROR; }
    const SCHC_Rule * defRule = nullptr;
    ctx.selected_rule = nullptr;
    const auto& rules = *ctx.rulesCtx;

    //iterating rules from the rule_candidate position (starts in 0 but changes if MO fails)
    for (size_t rule_candidate = ctx.search_position; rule_candidate< rules.size(); rule_candidate++) { 
        const auto & rule = rules[rule_candidate];

        if((!defRule //Searching for the default rule
            &&rule.getNatureType() == nature_type_t::NO_COMPRESSION)
            &&(rule.getRuleID() == ctx.default_ID)){
            defRule = &rule;
        }
        if (rule.getNatureType() != nature_type_t::COMPRESSION) continue;

        std::vector<bool> used(ctx.parsedPacket.size(), false);//Vector flags to verify if the field have already match
        bool rule_ok = true;

        for (const auto& entry : rule.getFields()) { //iterating entrys in the current rule
            bool found = false;

            for (size_t i = 0; i < ctx.parsedPacket.size(); ++i) { //iterating entrys (fields) in the parsed packet
                if (used[i]) continue;
                const auto& field = ctx.parsedPacket[i];

                if (entry.FID != field.fid) continue;
                if (!((entry.DI == direction_indicator_t::BI) || (entry.DI == ctx.direction))) continue;

                //For now, asuming all the package have one instance of each field (FP = 0 || FP = 1)

                used[i] = true;
                found = true;
                break;
            }

            if (!found) { rule_ok = false; break; }
        }

    if (rule_ok) { 
        ctx.search_position = rule_candidate + 1;
        ctx.selected_rule = &rule; 
        break; }
    }
    if (!ctx.selected_rule) {
        ctx.selected_rule = defRule;
    }

    ctx.next_state = COMP_STATE::COMP_STATE_MO;
    return STATE_RESULT::PASS;
}

static STATE_RESULT st_mo(FSM_Ctx& ctx) {
    std::vector<bool> used(ctx.parsedPacket.size(), false);//Vector flags to verify if the field have already been checked
    bool rule_ok = true;
    ctx.index_MM.clear();


    for (const auto& entry : ctx.selected_rule->getFields()) { 
        bool found = false;
        if(entry.MO == matching_operator_t::IGNORE_){
            found=true;
            continue;
        }
        for (size_t i = 0; i < ctx.parsedPacket.size(); ++i) { //iterating entrys (fields) in the parsed packet
            
            if (used[i]) continue;
            const auto& field = ctx.parsedPacket[i];
            if (entry.FID != field.fid) continue;
            switch (entry.MO)
            {
            case matching_operator_t::EQUAL_ :
                if(entry.TV.value_matrix[0] == field.value){//if not equal value    
                    found = true;
                    used[i] = true;
                }
                break;
                
            case matching_operator_t::MATCH_MAPPING_ :
                uint8_t pushed = 0;
                for(size_t j = 0; j< entry.TV.size ; j++){
                    if(entry.TV.value_matrix[j] == field.value){
                        pushed = static_cast<uint8_t>(j);
                        
                        break;
                    }
                        
                }
                if(pushed){
                    found = true;
                    used[i] = true;
                    ctx.index_MM.push_back(pushed); //push the index of the TV index that matches the packet value
                }
                
                break;
                
            case matching_operator_t::MSB_ ://Asuming all are FIXED cuz i didn't understand the var type
                if(entry.FL.type == "FIXED"){
                    if(msb_comparator(entry.TV.value_matrix[0], field.value, entry.MsbLength)){
                        found = true;
                        used[i] = true;
                    }
                //}else if((entry.FL.type != "FIXED")&&!(entry.MsbLength % 8)){ //If FL is variable,  MUST multiple of 

                }else{
                   found = false;
                }
                
                break;
            default:
                    ctx.next_state = COMP_STATE::COMP_STATE_IDLE;
                    return STATE_RESULT::ERROR;
                break;
            }

        }
        if(found) break;
    }
    if(!rule_ok){
        ctx.search_position = ctx.search_position + 1;
        ctx.selected_rule = nullptr;

        ctx.next_state = COMP_STATE::COMP_STATE_FIND_RULE;
        return STATE_RESULT::FAIL;
    }

    ctx.next_state = COMP_STATE::COMP_STATE_CDA;
    return STATE_RESULT::PASS;
}

static STATE_RESULT st_cda(FSM_Ctx& ctx) {
    
    std::vector<bool> used(ctx.parsedPacket.size(), false);//Vector flags to verify if the field have already been checked
    bool rule_ok = true;
    uint8_t mapp_index_count=0;
    
    for (const auto& entry : ctx.selected_rule->getFields()) { 
        bool found = false;

        for (size_t i = 0; i < ctx.parsedPacket.size(); ++i) { //iterating entrys (fields) in the parsed packet
            if (used[i]) continue;
            const auto& field = ctx.parsedPacket[i];
            

            switch (entry.CDA)
            {
            case cd_action_t::VALUE_SENT: //write the value of the packet
                ctx.residueBits.write_bits( field.value,field.bit_length);
                break;
            
            case cd_action_t::MAPPING_SENT:
                
                ctx.residueBits.write_bits(ctx.index_MM[mapp_index_count], 
                    value_size_in_bits(ctx.index_MM[mapp_index_count])); //to write the number in the least amount of bits
                mapp_index_count++;
                break;
            
            case cd_action_t::LSB:
                uint16_t lsb_value = lsb_extractor(field.value, (field.bit_length - entry.MsbLength));
                ctx.residueBits.write_bits(lsb_value, static_cast<uint8_t>(value_size_in_bits(lsb_value)));
                break;
            default:
                    found = true;
                    used[i] = true;
                break;
            }

        }
    }
    ctx.next_state = COMP_STATE::COMP_STATE_GEN;
    return STATE_RESULT::PASS;

}

static STATE_RESULT st_gen(FSM_Ctx& ctx) {

    ctx.out_SCHC.setPacketData(ctx.selected_rule->getRuleID(),
        ctx.residueBits);
    
    
    
    ctx.next_state = COMP_STATE::COMP_STATE_IDLE;
    return STATE_RESULT::PASS;
}

static STATE_RESULT st_decomp(FSM_Ctx& ctx) {
    
    ctx.next_state = COMP_STATE::COMP_STATE_IDLE;
    return STATE_RESULT::PASS;
}






