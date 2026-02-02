#ifndef SCHC_RULES_MANAGER.HPP
#define SCHC_RULES_MANAGER.HPP
#include "SCHC_RuleID.hpp"
#include <string>
#include <vector>
#include <unordered_map>

#include <nlohmann/json.hpp>    
//String to enum class types helpers
direction_indicator_t di_from_string(const std::string& s);
matching_operator_t mo_from_string(const std::string& s);
cd_action_t cda_from_string(const std::string& s);



std::vector<uint8_t> base64_to_hex(const std::string &in);

using json = nlohmann::json;
using RuleContext = std::unordered_map<uint32_t, SCHC_RuleID>;

RuleContext load_rules_from_json(const std::string &filename);
void printRuleContext(const RuleContext &ctx);


#endif