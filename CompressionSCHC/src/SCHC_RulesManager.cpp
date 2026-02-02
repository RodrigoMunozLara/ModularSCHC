

#include <fstream>
#include <iostream>
#include <vector>  
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include "SCHC_RuleID.hpp"
#include "SCHC_RulesManager.hpp"


//To load json file with nlohmann
using json = nlohmann::json;

json load_json_file(const std::string &filename){ //Function to read and load json file
    std::ifstream f(filename);
    if(!f.is_open()){
        throw std::runtime_error("No se pudo abrir el archivo JSON");

    }
    return json::parse(f);
}

//Auxiliar Functions to adapt the strings to enum types
direction_indicator_t di_from_string(const std::string& s) {
    if (s == "ietf-schc:di-uplink") return direction_indicator_t::UP;
    if (s == "ietf-schc:di-downlink") return direction_indicator_t::DOWN;
    if (s == "ietf-schc:di-bidirectional") return direction_indicator_t::BI;
    throw std::runtime_error("DI inválido: " + s);
}

matching_operator_t mo_from_string(const std::string& s) {
    if (s == "ietf-schc:mo-ignore") return matching_operator_t::IGNORE_;
    if (s == "ietf-schc:mo-equal") return matching_operator_t::EQUAL_;
    if (s == "ietf-schc:mo-match-mapping") return matching_operator_t::MATCH_MAPPING_;
    if (s == "ietf-schc:mo-msb") return matching_operator_t::MSB_;
    throw std::runtime_error("MO inválido: " + s);
}

cd_action_t cda_from_string(const std::string& s) {
    if (s == "not-sent") return cd_action_t::NOT_SENT;
    if (s == "value-sent") return cd_action_t::VALUE_SENT;
    if (s == "mapping-sent") return cd_action_t::MAPPING_SENT;
    if (s == "lsb") return cd_action_t::LSB;
    if (s == "compute-length") return cd_action_t::COMPUTE;
    throw std::runtime_error("CDA inválido: " + s);
}

//Decoder Base64:

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";
std::vector<uint8_t> base64_to_hex(const std::string& in) {
    std::vector<uint8_t> out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;

    int val = 0, valb = -8;
    for (uint8_t c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(uint8_t((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out; //returns a vector of bytes
}
//

using RuleContext = std::unordered_map<uint32_t, SCHC_RuleID>;

RuleContext load_rules_from_json(const std::string &filename ){

 json j = load_json_file(filename);
    RuleContext context;
    
    if (!j.contains("rule") || !j["rule"].is_array()) {
        throw std::runtime_error("El JSON no contiene un array 'rule'");
    }

    //Parsing a rule in json into a SCHC_RuleID

    for (const auto &jr : j["rule"]){ //recorrer los items dentro de la etiqueta rules
        uint32_t rule_id       = jr.at("rule_id").get<uint32_t>();
        uint8_t  rule_id_len   = jr.at("rule_id_length").get<uint8_t>();
        nature_type_t nature   = static_cast<nature_type_t>( //Casting type string into enum class
                                    jr.at("nature_type").get<int>());
        SCHC_RuleID rule(rule_id, rule_id_len, nature);

        if (!jr.contains("entry") || !j["rule"].is_array()) {
        throw std::runtime_error("The rule doesn't have entrys (FIDs)");
        }

        for(const auto &je : jr["entry"]){
            SCHC_Entry entry{};

            const std::string fid = je.at("field-id").get<std::string>();
            std::strncpy(entry.FID, fid.c_str(), sizeof(entry.FID)-1);
            entry.FID[sizeof(entry.FID-1)] = '\0';
            
            const auto &jfl = je.at("fiel-length");
            if (!jfl.is_number()) {
                entry.FL.bit_length = 0;
                entry.FL.type = jfl.get<std::string>();
            } else {
                entry.FL.bit_length = jfl.get<uint8_t>();
                entry.FL.type = "FIXED";
            }

            entry.FP = je.at("field-position").get<uint8_t>();;
            entry.DI = di_from_string(je.at("direction-indicator").get<std::string>());
            matching_operator_t aux = mo_from_string(je.at("matching-operator").get<std::string>());
            entry.MO = aux;
            if(static_cast<uint8_t>(aux) > 2){ //cast MO to verify if it is MSB
                std::string value_aux;
                value_aux = je.at("matching-operator-value")[0].at("value").get<std::string>();
                entry.MsbLength = base64_to_hex(value_aux)[0]; //MO-value always is one value
            }
            else
            {
                entry.MsbLength = 0;
            }
            
            entry.CDA = cda_from_string(je.at("comp-decomp-action").get<std::string>());
        
            rule.addField(entry);
        }
    }
    return context;
}

void printRuleContext(const RuleContext &ctx){
    std::cout << "Rules" << ctx.size()<<"\n";
    for (const auto & [rid, rule]:ctx){
        std::cout << "\n== Rule " << rid
                  << ") ==\n";
        rule.printRuleOut();
    }


}

SCHC_RuleID create_rule(uint32_t rule_id,
                        uint8_t rule_id_length,
                        nature_type_t nature,
                        const std::vector<SCHC_Entry>& entries) {
    SCHC_RuleID r(rule_id, rule_id_length, nature);
    for (const auto& e : entries) r.addField(e);
    return r;

}