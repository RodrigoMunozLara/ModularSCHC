#ifndef SCHC_RULES_MANAGER_HPP
#define SCHC_RULES_MANAGER_HPP
#include "SCHC_Rule.hpp"
#include <string>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <libyang/libyang.h>
#include <stdexcept>
#include <nlohmann/json.hpp>    
//String to enum class types helpers
direction_indicator_t di_from_string(const std::string& s);
matching_operator_t mo_from_string(const std::string& s);
cd_action_t cda_from_string(const std::string& s);


//Enum class to string helpers
std::string di_to_json(direction_indicator_t di);
std::string mo_to_json(matching_operator_t mo);
std::string cda_to_json(cd_action_t cda);
std::string nature_to_json(nature_type_t n);

//Decoder Base64
std::vector<uint8_t> base64_to_hex(const std::string &in);

//Encoder Base64
std::string bytes_to_base64(const std::vector<uint8_t>& data);

void validate_json_schc(const std::string& yang_dir,
                               const std::string& json_path) ;

using json = nlohmann::json;
using RuleContext = std::vector<SCHC_Rule>;

std::string input_line(const std::string& prompt);


void write_rule_to_json(const std::string& base_filename,
                        const std::string& out_filename,
                        const SCHC_Rule& rule);
                        
RuleContext load_rules_from_json(const std::string &filename);
void printRuleContext(const RuleContext &ctx);

void create_rule(SCHC_Rule &newrule);
void insert_rule_into_context(RuleContext& ctx, const SCHC_Rule& rule);


//-------------- Functions to check sizes  in memory ------------

std::size_t deep_size_string(const std::string& s);
// Estima memoria usada por vector<uint8_t> (buffer)
std::size_t deep_size_vec_u8(const std::vector<uint8_t>& v);
// Estima memoria deep de un SCHC_Entry
std::size_t deep_size_entry(const SCHC_Entry& e);

// Estima memoria deep de un SCHC_Rule
std::size_t deep_size_rule(const SCHC_Rule& r);

// Estima memoria deep de ctx = vector<SCHC_Rule>
std::size_t deep_size_ctx(const RuleContext& ctx);

void print_sizes(const RuleContext& ctx) ;



#endif