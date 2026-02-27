#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include <unordered_map>
#include "spdlog/spdlog.h"
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
static STATE_RESULT st_nocompress(FSM_Ctx& ctx);

// ---- ctor: asigna handlers ----
CompressorFSM::CompressorFSM() {
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_IDLE)]      = st_idle;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_PARSE)]     = st_parse;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_FIND_RULE)] = st_find_rule;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_MO)]        = st_mo;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_CDA)]       = st_cda;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_GEN)]       = st_gen;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_DECOMP)]    = st_decomp;
    handlers_[static_cast<size_t>(COMP_STATE::COMP_STATE_NOCOMPRESS)]  = st_nocompress;
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

uint16_t lsb_extractor(const std::vector<uint8_t> &value, uint16_t bits){    
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
        return STATE_RESULT::ERROR_;
    }

    ctx.next_state = state_;          // default: quedarse
    STATE_RESULT r = fn(ctx);         // ejecutar 1 vez

    if (r == STATE_RESULT::PASS_|| r == STATE_RESULT::FAIL_) {
        state_ = ctx.next_state;      // transicionar
    }
    return r;
}

STATE_RESULT CompressorFSM::runFSM(FSM_Ctx& ctx) {
    spdlog::info("Running FSM");
    state_ = COMP_STATE::COMP_STATE_IDLE;
    
    while(ctx.OnFSM) {
        STATE_RESULT r = stepFSM(ctx);
        if (r == STATE_RESULT::STOP_ || r == STATE_RESULT::ERROR_) {
            return r;
        }else if (r == STATE_RESULT::PASS_) {
            continue;
        } else if (r == STATE_RESULT::STAY_) {
            continue;
        } else if (r == STATE_RESULT::FAIL_) {
            continue;
        } else {
            ctx.error_code = 0xFF; // unknown result
            return STATE_RESULT::ERROR_;
        }
    }
    ctx.error_code = 0xFE; // loop guard
    return STATE_RESULT::ERROR_;
}


//----------------  State Functions ---------------------


//IDLE STATE: Waiting to change when a package arrives
//FOR NOW IT ASUMES IT IS A NON SCHC PACKET, JUSTP IPV6-UDP
static STATE_RESULT st_idle(FSM_Ctx& ctx) {
    spdlog::info("State: IDLE. Waiting for packet...");
    if(ctx.arrived){
        ctx.next_state = COMP_STATE::COMP_STATE_PARSE;
        return STATE_RESULT::PASS_;
    }
    else{
        
        return STATE_RESULT::STAY_;
    }
}

static STATE_RESULT st_parse(FSM_Ctx& ctx) {
    spdlog::info("State: PARSE. Parsing packet...");
    if (!ctx.raw_pkt) { ctx.error_code = 2; return STATE_RESULT::ERROR_; }

    bool link = (ctx.direction == direction_indicator_t::DOWN);

    //if(link){//If "DOWN" direction (to the device), parse for compression
            
        // parse IPv6+UDP -> vector<FieldValue>
        ctx.parsedPacketHeaders = parse_ipv6_udp_fields(*ctx.raw_pkt, link, ctx.out_SCHC);
        spdlog::debug("Parsed packet fields:");
        // build idx
        ctx.idx.clear();
        ctx.idx.reserve(ctx.parsedPacketHeaders.size());
        for (size_t i = 0; i < ctx.parsedPacketHeaders.size(); ++i) {
            ctx.idx.emplace(ctx.parsedPacketHeaders[i].fid, i);
        }
        log_field_values(ctx.parsedPacketHeaders);
        log_field_values(ctx.parsedPacketHeaders, spdlog::level::debug);
        spdlog::debug("Next State: FIND_RULE");
        ctx.next_state = COMP_STATE::COMP_STATE_FIND_RULE;
    //}
    /*else{ //if "UP" direction, parse for decompression. Not implemented yet.
        ctx.next_state = COMP_STATE::COMP_STATE_DECOMP;
    }*/

    if (ctx.parsedPacketHeaders.empty()) return STATE_RESULT::FAIL_;

    
    return STATE_RESULT::PASS_;
}

static STATE_RESULT st_find_rule(FSM_Ctx& ctx) {
    spdlog::info("State: FIND_RULE. Searching for matching rule...");
    if (!ctx.rulesCtx) { 
        ctx.error_code = 3;
        spdlog::error("No rules context available in FSM_Ctx. ERROR {}", ctx.error_code);
        return STATE_RESULT::ERROR_; 
    
    }
    
    ctx.selected_rule = nullptr;
    const auto& rules = *ctx.rulesCtx;

    //iterating rules from the rule_candidate position (starts in 0 but changes if MO fails)
    for (size_t rule_candidate = ctx.search_position; rule_candidate< rules.size(); rule_candidate++) { 
        const auto & rule = rules[rule_candidate];
        spdlog::debug("Evaluating rule with ID={} and nature type={}", rule.getRuleID(), static_cast<int>(rule.getNatureType()));

        if((!ctx.defRule //Searching for the default rule
            &&rule.getNatureType() == nature_type_t::NO_COMPRESSION)
            &&(rule.getRuleID() == ctx.default_ID)){
            ctx.defRule = &rule;
            spdlog::info("Default rule found with ID={}", ctx.defRule->getRuleID());
        }
        if (rule.getNatureType() != nature_type_t::COMPRESSION) {
            spdlog::debug("Skipping rule with ID={}", rule.getRuleID());
            continue;//Only compression rules are valid for this state
        }
        std::vector<bool> used(ctx.parsedPacketHeaders.size(), false);//Vector flags to verify if the field have already match
        bool rule_ok = true;
        spdlog::debug("\nEntrys in the rule:");
        for (const auto& entry : rule.getFields()) { //iterating entrys in the current rule
            bool found = false;
            
            for (size_t i = 0; i < ctx.parsedPacketHeaders.size(); ++i) { //iterating entrys (fields) in the parsed packet
                if (used[i]) continue;
                const auto& field = ctx.parsedPacketHeaders[i];
                spdlog::debug("Comparing rule entry FID={} and DI={} with packet field FID={} and DI={}",
                         entry.FID, static_cast<int>(entry.DI), field.fid, static_cast<int>(ctx.direction));

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
        spdlog::info("Matching rule found with ID={}", ctx.selected_rule->getRuleID());
        break; }
    }
    if (!ctx.selected_rule) {
        if(!ctx.defRule){
            ctx.error_code = 4;
            spdlog::error("No matching compression rule found and no default rule available. ERROR {}", ctx.error_code);
            return STATE_RESULT::ERROR_;
        }
        ctx.selected_rule = ctx.defRule;
        ctx.next_state = COMP_STATE::COMP_STATE_NOCOMPRESS;
        spdlog::warn("No matching compression rule found. Using default rule with ID={}", ctx.defRule->getRuleID());
        return STATE_RESULT::PASS_;
    }

    ctx.next_state = COMP_STATE::COMP_STATE_MO;
    spdlog::info("Next State: MO");
    return STATE_RESULT::PASS_;
}

static STATE_RESULT st_nocompress(FSM_Ctx& ctx){
    spdlog::info("State: NO_COMPRESS. Generating uncompressed packet...");
    ctx.residueBits.clear();
    //write the whole packet as residue, because it is not compressible
    ctx.residueBits.write_msb(*ctx.raw_pkt,
                             static_cast<uint32_t>(ctx.raw_pkt->size() * 8u));
    spdlog::debug("writed the original raw packet of {} bytes", ctx.raw_pkt->size());
    //Save RuleID and Residue in the output packet instance
    ctx.out_SCHC.setPacketData(ctx.selected_rule->getRuleID(), ctx.residueBits);
    
    //write the RuleID in the compressed packet
    ctx.totalPacket.clear();
    ctx.totalPacket.write_uint<uint32_t>(
        ctx.selected_rule->getRuleID(),
        static_cast<uint32_t>(ctx.selected_rule->getRuleIDLength())
    );

    //write the whole packet as residue
    ctx.totalPacket.write_writer(ctx.residueBits);

    // Padding a byte
    if ((ctx.totalPacket.bitLength() & 7u) != 0u) {
        uint32_t pad = 8u - (ctx.totalPacket.bitLength() & 7u);
        ctx.padding_bits = static_cast<uint8_t>(pad);
        ctx.totalPacket.write_u64(0u, pad); // escribe pad bits en 0
    } else {
        ctx.padding_bits = 0;
    }
    spdlog::debug("Packet length after padding: {} bits ({} bytes)", ctx.totalPacket.bitLength(), ctx.totalPacket.bitLength() / 8u );

    ctx.out_SCHC.setPacketRaw(ctx.totalPacket);

    ctx.next_state = COMP_STATE::COMP_STATE_IDLE;
    spdlog::info("Packet generated without compression. Next State: IDLE");
    ctx.arrived = false; //reset arrived flag for the next packet
    return STATE_RESULT::PASS_;
}

static STATE_RESULT st_mo(FSM_Ctx& ctx) {
    spdlog::info("State: MO. Evaluating Matching Operators...");
    std::vector<bool> used(ctx.parsedPacketHeaders.size(), false);//Vector flags to verify if the field have already been checked
    bool rule_ok = true;
    ctx.index_MM.clear();

    for (const auto& entry : ctx.selected_rule->getFields()) { 
        spdlog::debug("Evaluating entry with FID={} and MO={}", entry.FID, static_cast<int>(entry.MO));
        bool found = false;//For each entry in the rule, check if there is a match in the packet with the corresponding MO.
        // If not, the rule is not valid for that packet and we have to try with the next one. 
        //If there is a match, we save the index of the packet field that matches with that entry to use it later in the CDA state.
        if(entry.MO == matching_operator_t::IGNORE_){
            found=true;
            continue;
        }
        for (size_t i = 0; i < ctx.parsedPacketHeaders.size(); ++i) { //iterating entrys (fields) in the parsed packet
            if (used[i]) continue;
            const auto& field = ctx.parsedPacketHeaders[i];
            spdlog::debug("Comparing rule entry FID={}, MO={} and TV with \n packet field FID={} ", entry.FID, static_cast<int>(entry.MO) ,field.fid);
            if (entry.FID != field.fid) continue;
            switch (entry.MO)
            {
            case matching_operator_t::EQUAL_ :{
                spdlog::debug("Comparing Values. Rule value: {}, Packet value: {}", bytes_to_hex(entry.TV.value_matrix[0]), bytes_to_hex(field.value));
                if(entry.TV.value_matrix[0] == field.value){ 
                    
                    found = true;
                    used[i] = true;
                }
                break;
            }
            case matching_operator_t::MATCH_MAPPING_ :{
                int8_t pushed = -1;
                for(size_t j = 0; j< entry.TV.size ; j++){
                    if(entry.TV.value_matrix[j] == field.value){
                        spdlog::debug("Comparing Values. Rule value: {}, Packet value: {}", bytes_to_hex(entry.TV.value_matrix[j]), bytes_to_hex(field.value));
                        pushed = static_cast<uint8_t>(j);
                        spdlog::debug("Match-Mapping found for FID={} at TV index={}", entry.FID, pushed);
                        break;
                    }
                        
                }
                if(pushed>=0){
                    found = true;
                    used[i] = true;
                    ctx.index_MM.push_back(pushed); //push the index of the TV index that matches the packet value
                }else{
                    found = false;
                    used[i] = true;
                }
                
                break;
            }    
            case matching_operator_t::MSB_ :{//Asuming all are FIXED cuz i didn't understand the var type
                if(entry.FL.type == "FIXED"){
                    spdlog::debug("Comparing Values. Rule value: {}, Packet value: {}", bytes_to_hex(entry.TV.value_matrix[0]), bytes_to_hex(field.value));
                    if(msb_comparator(entry.TV.value_matrix[0], field.value, entry.MsbLength)){
                        found = true;
                        used[i] = true;
                        spdlog::debug("MSB match found for FID={} with MSB length={}", entry.FID, entry.MsbLength);
                    }
                //}else if((entry.FL.type != "FIXED")&&!(entry.MsbLength % 8)){ //If FL is variable,  MUST multiple of 

                }else{
                   found = false;
                }
                
                break;
            }
            default:
                    ctx.next_state = COMP_STATE::COMP_STATE_IDLE;
                    found = false;
                    spdlog::error("Unknown MO, returning to IDLE. MO value: {}", static_cast<int>(entry.MO));
                    return STATE_RESULT::ERROR_;
                break;
            }

            if(found) break;
            

        }
        if(!found){
            rule_ok = false;
            spdlog::debug("No match found for entry with FID={} and MO={}. Rule rejected.", entry.FID, static_cast<int>(entry.MO));
            break;
        }

    }
    if(!rule_ok){
        ctx.selected_rule = nullptr;
        spdlog::info("Rule rejected. Next State: FIND_RULE");
        ctx.next_state = COMP_STATE::COMP_STATE_FIND_RULE;
        return STATE_RESULT::FAIL_; 
    }
    spdlog::info("All entries matched. Rule accepted. Next State: CDA");
    ctx.next_state = COMP_STATE::COMP_STATE_CDA;
    return STATE_RESULT::PASS_;
}

static STATE_RESULT st_cda(FSM_Ctx& ctx) {
    spdlog::info("State: CDA. Evaluating Compression/Decompression Actions...");
    std::vector<bool> used(ctx.parsedPacketHeaders.size(), false);
    uint32_t mapp_index_count = 0;

    ctx.residueBits.clear();

    for (const auto& entry : ctx.selected_rule->getFields()) {
        bool found = false;

        for (size_t i = 0; i < ctx.parsedPacketHeaders.size(); ++i) {
            if (used[i]) continue;

            const auto& field = ctx.parsedPacketHeaders[i];
            if (entry.FID != field.fid) continue;

            switch (entry.CDA) {//The only three CDAs that generate residue are VALUE_SENT, MAPPING_SENT and LSB. 
                //The rest of CDAs just indicate that the field is not sent but doesn't generate residue.

            case cd_action_t::VALUE_SENT:
                ctx.residueBits.write_msb(field.value,
                                         static_cast<uint32_t>(field.bit_length));
                spdlog::debug("VALUE_SENT for FID={} with bit length={}", entry.FID, field.bit_length);
                found = true;
                used[i] = true;
                break;

            case cd_action_t::MAPPING_SENT: {
                uint32_t aux = static_cast<uint32_t>(entry.TV.value_matrix.size());//number of possible values in the TV
                uint32_t idx_bits = static_cast<uint32_t>(value_size_in_bits(aux - 1u));//number of bits needed to represent the index of the TV value
                
                ctx.residueBits.write_uint<uint32_t>(
                    static_cast<uint32_t>(ctx.index_MM[mapp_index_count]),
                    idx_bits
                );
                spdlog::debug("MAPPING_SENT for FID={} with TV size={} and index bits={}", entry.FID, aux, idx_bits);
                ++mapp_index_count;//to save different index for each field with Match-Mapping MO
                found = true;
                used[i] = true;
                break;
            }

            case cd_action_t::LSB: {
                uint32_t lsb_bits = static_cast<uint32_t>(field.bit_length - entry.MsbLength);
                uint64_t lsb_value = static_cast<uint64_t>(
                    lsb_extractor(field.value, static_cast<uint16_t>(lsb_bits))
                );

                ctx.residueBits.write_u64(lsb_value, lsb_bits);
                spdlog::debug("LSB for FID={} with LSB bits={}", entry.FID, lsb_bits);
                found = true;
                used[i] = true;
                break;
            }

            default: //for other CDAs
                spdlog::debug("CDA {} for FID={} does not generate residue bits", static_cast<int>(entry.CDA), entry.FID);
                found = true;
                used[i] = true;
                break;
            }

            if (found) break;
        }
    }
    spdlog::info("CDA evaluation completed. Next State: GEN");
    ctx.next_state = COMP_STATE::COMP_STATE_GEN;
    return STATE_RESULT::PASS_;
}

static STATE_RESULT st_gen(FSM_Ctx& ctx) {
    spdlog::info("State: GEN. Generating compressed packet...");
    ctx.totalPacket.clear();

    //Save and write the RuleID found in the output packet instance and in the raw packet
    spdlog::debug("Writing RuleID={} with length={} bits in the compressed packet", ctx.selected_rule->getRuleID(), ctx.selected_rule->getRuleIDLength());
    ctx.out_SCHC.setPacketData(ctx.selected_rule->getRuleID(), ctx.residueBits);
    ctx.totalPacket.write_uint<uint32_t>(
        static_cast<uint32_t>(ctx.selected_rule->getRuleID()),
        static_cast<uint32_t>(ctx.selected_rule->getRuleIDLength())
    );

    //Write the residue bits in the raw packet
    ctx.totalPacket.write_writer(ctx.residueBits);
    spdlog::debug("Written residue bits of length {} bits", ctx.residueBits.bitLength());
    ctx.totalPacket.write_bytes_aligned(ctx.out_SCHC.getPayload());
    // Padding byte
    if ((ctx.totalPacket.bitLength() & 7u) != 0u) {
        uint32_t pad = 8u - (ctx.totalPacket.bitLength() & 7u);
        ctx.padding_bits = static_cast<uint8_t>(pad);
        ctx.totalPacket.write_u64(0u, pad);
    } else {
        ctx.padding_bits = 0;
    }

    //Save the raw packet in the output packet instance
    ctx.out_SCHC.setPacketRaw(ctx.totalPacket);
    ctx.arrived = false; //reset arrived flag for the next packet
    spdlog::info("Compressed packet generated. Next State: IDLE. Waiting for next packet...");
    ctx.out_SCHC.printResidue(); //Print residue details after generation
    ctx.out_SCHC.printPacket(); //Print packet details after generation
    ctx.next_state = COMP_STATE::COMP_STATE_IDLE;
    return STATE_RESULT::PASS_;
}


static STATE_RESULT st_decomp(FSM_Ctx& ctx) {
    
    ctx.next_state = COMP_STATE::COMP_STATE_IDLE;
    return STATE_RESULT::PASS_;
}






