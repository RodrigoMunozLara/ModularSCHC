
#ifndef SCHC_COMPRESSOR_HPP
#define SCHC_COMPRESSOR_HPP



#include <cstdint>
#include <vector>
#include <array>
#include <unordered_map>
#include "SCHC_Rule.hpp"
#include "SCHC_RulesManager.hpp"
#include "PacketParser.hpp"
#include "SCHC_Packet.hpp"



// Rule comparator state machine for SCHC Rules
enum class RULE_TYPE : uint8_t{
    NO_DEFAULT_RULE = 0,
    DEFAULT_RULE = 1
};

enum class COMP_STATE : uint8_t{
    COMP_STATE_IDLE,
    COMP_STATE_PARSE, 
    COMP_STATE_FIND_RULE,
    COMP_STATE_MO,
    COMP_STATE_CDA, 
    COMP_STATE_GEN, 
    COMP_STATE_DECOMP,
    COMP_STATE_NOCOMPRESS
};

enum class STATE_RESULT:uint8_t{
    PASS_,
    STAY_, 
    FAIL_,
    STOP_,
    ERROR_
};

struct FSM_Ctx{ //Context for de FSM

    RuleContext *rulesCtx = nullptr; //Pointer to Rules
    //std::unordered_map<uint8_t rule_index , const SCHC_Rule* ptr_rule> decompress_map;
    bool arrived=false;
    const std::vector<uint8_t> *raw_pkt = nullptr; //pointer to original packet
    direction_indicator_t direction = direction_indicator_t::BI; //direction of the original packet
    
    uint32_t default_ID = 0;
    const SCHC_Rule *selected_rule = nullptr; //pointer to selected rule
    size_t search_position = 0; //To go to the last rule found if that rule doesn't pass MO


    std::vector<FieldValue> parsedPacketHeaders; //parsed original packet
    std::unordered_map<std::string, std::size_t> idx; // hash table to link FID of the packet (for compression) with position
    
    const FieldValue* getField(const std::string& fid) const { //helper to get the FID of the packet
        auto it = idx.find(fid);
        if (it == idx.end()) return nullptr;
        return &parsedPacketHeaders[it->second];
    }

    //---------- Output -----------
    std::vector<uint8_t> index_MM; //vector of index for Match-Mapping if more than one field has that MO
    BitWriter residueBits;
    BitWriter totalPacket;
    uint8_t padding_bits=0;
    SCHC_Compressed_Packet out_SCHC;

    //--------- FSM Control --------
    COMP_STATE next_state = COMP_STATE::COMP_STATE_IDLE;
    bool OnFSM = true; //flag to control the FSM loop
    uint8_t error_code =0 ;

};



using StateFn = STATE_RESULT(*)(FSM_Ctx&);//pointer to function: use FSM_Ctx and returns state_result

class CompressorFSM{

    public:
        CompressorFSM();
        STATE_RESULT stepFSM(FSM_Ctx &ctx);
        STATE_RESULT runFSM(FSM_Ctx &ctx);

    private:
        static constexpr std::size_t N = static_cast<std::size_t>(COMP_STATE::COMP_STATE_NOCOMPRESS) + 1;
        COMP_STATE state_ = COMP_STATE::COMP_STATE_IDLE;
        
        std::array<StateFn , N> handlers_{};

};



#endif