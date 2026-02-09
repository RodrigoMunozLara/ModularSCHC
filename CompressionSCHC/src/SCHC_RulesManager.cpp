

#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>  
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <cctype> 
#include <algorithm>
#include "SCHC_Rule.hpp"
#include "SCHC_RulesManager.hpp"




//Auxiliar Functions to adapt the strings to enum types
direction_indicator_t di_from_string(const std::string& s) {
    if (s == "ietf-schc:di-up"|| s == "up") return direction_indicator_t::UP;
    if (s == "ietf-schc:di-down"|| s == "down") return direction_indicator_t::DOWN;
    if (s == "ietf-schc:di-bidirectional"|| s == "bidirectional" || s == "bi") return direction_indicator_t::BI;
    spdlog::error("DI inválido: {}" , s);
    throw std::runtime_error("DI inválido: " + s);
}

matching_operator_t mo_from_string(const std::string& s) {
    if (s == "ietf-schc:mo-ignore" || s == "ignore") return matching_operator_t::IGNORE_;
    if (s == "ietf-schc:mo-equal"|| s == "equal") return matching_operator_t::EQUAL_;
    if (s == "ietf-schc:mo-match-mapping"|| s == "match-mapping") return matching_operator_t::MATCH_MAPPING_;
    if (s == "ietf-schc:mo-msb"|| s == "msb") return matching_operator_t::MSB_;
    spdlog::error("MO inválido: {}" , s);
    throw std::runtime_error("MO inválido: " + s);
}

cd_action_t cda_from_string(const std::string& s) {
    if (s == "ietf-schc:cda-not-sent" || s == "not-sent") return cd_action_t::NOT_SENT;
    if (s == "ietf-schc:cda-value-sent" || s == "value-sent") return cd_action_t::VALUE_SENT;
    if (s == "ietf-schc:cda-mapping-sent"|| s == "mapping-sent") return cd_action_t::MAPPING_SENT;
    if (s == "ietf-schc:cda-lsb" || s == "lsb") return cd_action_t::LSB;
    if (s == "ietf-schc:cda-compute" || s == "compute") return cd_action_t::COMPUTE;
    spdlog::error("CDA inválido: {}",s);
    throw std::runtime_error("CDA inválido: " + s);
}

nature_type_t nature_from_string(const std::string& s) {
    if (s == "ietf-schc:nature-compression" || s == "compression") return nature_type_t::COMPRESSION;
    if (s == "ietf-schc:nature-no-compression" || s == "no-compression") return nature_type_t::NO_COMPRESSION;
    if (s == "ietf-schc:nature-fragmentation" || s =="fragmentation") return nature_type_t::FRAGMENTATION;
    spdlog::error("Nature Type inválido: {}", s);
    throw std::runtime_error("Nature Type inválido: " + s);
}
//-----------------------------------------------------------------------------------//

//Auxiliar functions to adapt enum types to string
std::string di_to_json(direction_indicator_t di) {
    switch (di) {
        case direction_indicator_t::UP:
            return "ietf-schc:di-up";
        case direction_indicator_t::DOWN:
            return "ietf-schc:di-down";
        case direction_indicator_t::BI:
            return "ietf-schc:di-bidirectional";
    }
    spdlog::error("DI inválido (enum desconocido)");
    throw std::runtime_error("DI inválido (enum desconocido)");
}

std::string mo_to_json(matching_operator_t mo) {
    switch (mo) {
        case matching_operator_t::IGNORE_:
            return "ietf-schc:mo-ignore";
        case matching_operator_t::EQUAL_:
            return "ietf-schc:mo-equal";
        case matching_operator_t::MATCH_MAPPING_:
            return "ietf-schc:mo-match-mapping";
        case matching_operator_t::MSB_:
            return "ietf-schc:mo-msb";
    }

    spdlog::error("MO inválido (enum desconocido)");
    throw std::runtime_error("MO inválido (enum desconocido)");
}

std::string cda_to_json(cd_action_t cda) {
    switch (cda) {
        case cd_action_t::NOT_SENT:
            return "ietf-schc:cda-not-sent";
        case cd_action_t::VALUE_SENT:
            return "ietf-schc:cda-value-sent";
        case cd_action_t::MAPPING_SENT:
            return "ietf-schc:cda-mapping-sent";
        case cd_action_t::LSB:
            return "ietf-schc:cda-lsb";
        case cd_action_t::COMPUTE:
            return "ietf-schc:cda-compute-length";
    }
    spdlog::error("CDA inválido (enum desconocido)");
    throw std::runtime_error("CDA inválido (enum desconocido)");
}

std::string nature_to_json(nature_type_t n) {
    switch (n) {
        case nature_type_t::COMPRESSION:
            return "ietf-schc:nature-compression";
        case nature_type_t::NO_COMPRESSION:
            return "ietf-schc:nature-no-compression";
        case nature_type_t::FRAGMENTATION:
            return "ietf-schc:nature-fragmentation";
    }
    spdlog::error("Nature Type inválido (enum desconocido)");
    throw std::runtime_error("Nature Type inválido (enum desconocido)");
}
//----------------------------------------------------------------------------//

//To load json file with nlohmann
using json = nlohmann::json;

json load_json_file(const std::string &filename){ //Function to read and load json file
    std::ifstream f(filename);
    if(!f.is_open()){
        spdlog::error("No se pudo abrir el archivo JSON");
        throw std::runtime_error("No se pudo abrir el archivo JSON");

    }
    return json::parse(f);
}
//to save rules to a json file
void write_rule_to_json(const std::string& base_filename,
                        const std::string& out_filename,
                        const SCHC_Rule& rule)
{
    json j;

    try {
        j = load_json_file(base_filename);
    } catch (...) {
        j = json::object();
    }

    //Verify the root of the model
    if (!j.contains("ietf-schc:schc") || !j["ietf-schc:schc"].is_object()) {
        j["ietf-schc:schc"] = json::object();
    }
    if (!j["ietf-schc:schc"].contains("rule") || !j["ietf-schc:schc"]["rule"].is_array()) {
        j["ietf-schc:schc"]["rule"] = json::array();
    }

    //Write Rule
    json jr;
    jr["rule-id-value"]  = rule.getRuleID();
    jr["rule-id-length"] = rule.getRuleIDLength();
    jr["rule-nature"]    = nature_to_json(rule.getNatureType());

    //Entries of the rule
    jr["entry"] = json::array();

    for (const auto& e:   rule.getFields()) {
        json je;

        je["field-id"]            = std::string(e.FID);
        je["field-position"]      = e.FP;
        je["direction-indicator"] = di_to_json(e.DI);


        if (e.FL.type == "FIXED") {
            je["field-length"] = e.FL.bit_length;
        } else {
            je["field-length"] = e.FL.type;
        }

        //Target Value List
        je["target-value"] = json::array();

        const size_t n = std::min<size_t>(e.TV.index, e.TV.value_matrix.size());

        for (uint16_t idx = 0; idx < static_cast<uint16_t>(n); idx++)
        {
            json tv;
            tv["index"] = idx;
            //Se guarda el item de la matriz como un solo string base64
            tv["value"] = bytes_to_base64(e.TV.value_matrix[idx]);
            je["target-value"].push_back(tv);
        }
    

        // matching-operator
        je["matching-operator"] = mo_to_json(e.MO);

        // matching-operator-value:
        if (e.MO == matching_operator_t::MSB_) {
            je["matching-operator-value"] = json::array();

            json mov;
            mov["index"] = 0;

            // MSB length saved in base64
            std::vector<uint8_t> msb_bytes = { static_cast<uint8_t>(e.MsbLength) };
            mov["value"] = bytes_to_base64(msb_bytes);

            je["matching-operator-value"].push_back(mov);
        }

        // comp-decomp-action
        je["comp-decomp-action"] = cda_to_json(e.CDA);

        jr["entry"].push_back(je);
    }
    //Append
    j["ietf-schc:schc"]["rule"].push_back(jr);

    //Write in the temporal file
    std::ofstream out(out_filename, std::ios::trunc);
    if (!out.is_open()) {
        spdlog::error("No se pudo abrir JSON para escritura: {}", out_filename);
        throw std::runtime_error("No se pudo abrir JSON para escritura: " + out_filename);
    }
    spdlog::info("Se escribió la regla nueva en el archivo json {}", out_filename);
    out << j.dump(2);
    
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
    for (uint8_t c:   in) {
        if (c == '=' || std::isspace(c)) continue;
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
//Encoder base64:
std::string bytes_to_base64(const std::vector<uint8_t>& data) {
    static const char* b64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c:   data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    //returns a base64 string of the uint8_t type vector value
    return out;
}
//Function to parse string hex input to a vector of bytes
static std::vector<uint8_t> parse_hex_bytes(std::string s) {
    // Caso A: formato tipo "0xaa 0x4e" o "aa 4e"
    // Caso B: formato pegado tipo "aa4e"
    // Caso C: con comas / corchetes "[0xaa, 0x4e]"

    // Normalizamos: dejamos separadores como espacio
    for (char& c : s) {
        if (c == '[' || c == ']' || c == ',' || c == ';') c = ' ';
    }

    // Si viene con tokens (espacios), parseamos token a token
    std::vector<uint8_t> out;
    std::stringstream ss(s);
    std::string tok;

    bool had_tokens = false;
    while (ss >> tok) {
        had_tokens = true;

        // quitar "0x" o "0X"
        if (tok.rfind("0x", 0) == 0 || tok.rfind("0X", 0) == 0) {
            tok = tok.substr(2);
        }

        // Si el token no es hex, lo saltamos
        if (tok.empty() || !std::all_of(tok.begin(), tok.end(), [](unsigned char ch){
            return std::isxdigit(ch);
        })) {
            continue;
        }

        // Soportar token de 1 o 2+ hex: si es 1 hex -> 0?x
        if (tok.size() == 1) tok = "0" + tok;

        // Si el token tiene más de 2 hex, lo interpretamos como bytes pegados (aa4e -> aa 4e)
        if (tok.size() > 2) {
            if (tok.size() % 2 != 0) {
                throw std::runtime_error("Hex inválido (cantidad impar de dígitos): " + tok);
            }
            for (size_t i = 0; i < tok.size(); i += 2) {
                auto byte_str = tok.substr(i, 2);
                uint8_t v = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
                out.push_back(v);
            }
        } else {
            uint8_t v = static_cast<uint8_t>(std::stoul(tok, nullptr, 16));
            out.push_back(v);
        }
    }

    // Si no habían tokens, intentamos “pegado” (ej: "aa4e")
    if (!had_tokens) {
        // limpiar a sólo hex
        std::string hex;
        for (unsigned char c : s) {
            if (std::isxdigit(c)) hex.push_back(static_cast<char>(c));
        }
        if (hex.empty()) return {};
        if (hex.size() % 2 != 0) {
            throw std::runtime_error("Hex inválido (cantidad impar de dígitos): " + hex);
        }
        for (size_t i = 0; i < hex.size(); i += 2) {
            uint8_t v = static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16));
            out.push_back(v);
        }
    }

    return out;
}


//---------------------------------------------------------------------------//
using RuleContext = std::unordered_map<uint32_t, SCHC_Rule>;

RuleContext load_rules_from_json(const std::string &filename ){

 json j = load_json_file(filename);
    RuleContext context;
    
    
    if (!j.contains("ietf-schc:schc") || !j["ietf-schc:schc"].is_object()) {
        spdlog::error("El JSON no posee un contenedor 'ietf-schc:schc'");
        throw std::runtime_error("El JSON no posee un contenedor 'ietf-schc:schc'");
    }

    auto &root = j["ietf-schc:schc"];
    if (!root.contains("rule") || !root["rule"].is_array()) {
        spdlog::error("El JSON no contiene un array 'rule'");
        throw std::runtime_error("El JSON no contiene un array 'rule'");
    }

    //Parsing a rule in json into a SCHC_Rule

    for (const auto &jr:   root["rule"]){ //recorrer los items dentro de la etiqueta rules
        uint32_t rule_id       = jr.at("rule-id-value").get<uint32_t>();
        uint8_t  rule_id_len   = jr.at("rule-id-length").get<uint8_t>();
        nature_type_t nature   = nature_from_string(jr.at("rule-nature").get<std::string>());
        SCHC_Rule rule(rule_id, rule_id_len, nature);

        if (!jr.contains("entry") && (rule_id != 0)) {
            spdlog::error("The rule doesn't have entrys (FIDs)");
            throw std::runtime_error("The rule doesn't have entrys (FIDs)");
        }
        else if (!jr.contains("entry") && (rule_id == 0)) {
            spdlog::info("RuleID = 0, Rule default");
            continue;
        }

        for(const auto &je:   jr["entry"]){
            SCHC_Entry entry{};

            const std::string fid = je.at("field-id").get<std::string>();
            std::strncpy(entry.FID, fid.c_str(), sizeof(entry.FID)-1);
            entry.FID[sizeof(entry.FID)-1] = '\0';
            
            const auto &jfl = je.at("field-length");
            if (!jfl.is_number()) {
                entry.FL.bit_length = 0;
                entry.FL.type = jfl.get<std::string>();
            } else {
                entry.FL.bit_length = jfl.get<uint8_t>();
                entry.FL.type = "FIXED";
            }

            entry.FP = je.at("field-position").get<uint8_t>();;
            entry.DI = di_from_string(je.at("direction-indicator").get<std::string>());
            
    
            entry.TV.value_matrix.clear();
            if (je.contains("target-value")) {
                const auto& tv_arr = je.at("target-value");
                if (!tv_arr.is_array()) {
                    spdlog::error("target-value debe ser array");
                    throw std::runtime_error("target-value debe ser array");
                }

                // Ordenar por index por si viene desordenado
                std::vector<std::pair<uint16_t, std::vector<uint8_t>>> tv_items;
                tv_items.reserve(tv_arr.size());

                for (const auto& tv:   tv_arr) {
                    uint16_t idx = tv.at("index").get<uint16_t>();
                    std::string b64 = tv.at("value").get<std::string>();

                    auto bytes = base64_to_hex(b64);
                    if (bytes.empty()) {
                        spdlog::error("target-value.value (binary) vacío/ inválido");
                        throw std::runtime_error("target-value.value (binary) vacío/ inválido");
                    }
                    tv_items.push_back({idx, std::move(bytes)});
                }

                std::sort(tv_items.begin(), tv_items.end(),
                          [](auto& a, auto& b){ return a.first < b.first; });

                entry.TV.value_matrix.reserve(tv_items.size());
                for (auto& p:   tv_items) {
                    entry.TV.value_matrix.push_back(std::move(p.second));
                }
                entry.TV.index = static_cast<uint16_t>(entry.TV.value_matrix.size());
            } else {
                entry.TV.index = 0;
            }

            
            matching_operator_t aux = mo_from_string(je.at("matching-operator").get<std::string>());
            entry.MO = aux;
            if (aux == matching_operator_t::MSB_) { //Verify that the rule has matching-operator-value if MSB
                if (!je.contains("matching-operator-value") || !je["matching-operator-value"].is_array()) {
                    spdlog::error("MO=MSB pero falta matching-operator-value (YANG must)");
                    throw std::runtime_error("MO=MSB pero falta matching-operator-value (YANG must)");
                }

                const auto& mov_arr = je.at("matching-operator-value");
                if (mov_arr.empty()) {
                    spdlog::error("matching-operator-value vacío");
                    throw std::runtime_error("matching-operator-value vacío");
                }

                // 
                const auto& mov0 = mov_arr.at(0);
                std::string b64 = mov0.at("value").get<std::string>();
                auto bytes = base64_to_hex(b64);
                entry.MsbLength = bytes.empty() ? 0:   bytes[0];
            } else {
                entry.MsbLength = 0;
            }
            
            entry.CDA = cda_from_string(je.at("comp-decomp-action").get<std::string>());
        
            rule.addField(entry);
        }
        auto [it, inserted] =
            context.emplace(rule.getRuleID(), std::move(rule));

        if (!inserted) {
            spdlog::error("RuleID duplicado en JSON: {}", rule_id);
            throw std::runtime_error( 
                "RuleID duplicado en JSON: " + std::to_string(rule_id));
            }
        
    }
    return context;
}

void printRuleContext(const RuleContext &ctx){
    std::cout << "Rules" << ctx.size()<<"\n";
    for (const auto & [rid, rule]:ctx){
        std::cout << "\n== Rule " << rid
                  << " ==\n";
        rule.printRuleOut();
    }


}

//Auxiliar input function
std::string input_line(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

void create_rule(SCHC_Rule &newrule) {
    
    std::cout << "|--- Ingrese los campos de la nueva regla en siguiendo el modelo YANG SCHC ---| \n";
    std::cout << "  numero id \n"
                <<"  largo id  \n"
                << "  ietf-schc:nature-type' (compression/no-compression/fragmentation)\n" 
                << "  ietf-schc:fid-type (ipv6-type/udp-type/coap-type) \n"
                << "  ietf-schc:di-type (up/down/bidirectional) \n"
                << "  valores para TV \n"
                << "  ietf-schc:mo-type (equal/ignore/msb/match-mapping) \n"        
                << "  ietf-schc:cda-type  (value-sent/compute/appiid/deviid/not-sent/lsb/mapping-sent)\n";
    uint32_t newID = static_cast<uint32_t>(std::stoul(input_line("Ingrese el ID para la nueva regla:  ")));
    uint8_t newrule_id_length = static_cast<uint8_t>(std::stoul(input_line("Ingrese rule ID Length in bits:  "))); 
    nature_type_t newnature;
    newnature  = nature_from_string((input_line("|-- Ingrese Nature Type :  ")));
    

    size_t n = 0;
    while (n == 0) {
        n = static_cast<size_t>(std::stoul(input_line("|-- Ingrese cantidad de Field Descriptors (>=1):  ")));
        if (n == 0) std::cout << "Debe ser >= 1.\n";
    }

    std::vector<SCHC_Entry> entries;
    entries.reserve(n);

    for(size_t i= 0 ; i<n ;i++){
            std:: cout << "\n+\nField Descriptor n°:" << i;
            SCHC_Entry entry{};
            std::cout << "\n |-- Ingrese Field Identifier en minúscula y separado con '-'";
            std::string auxiliar_in;
            if(i==0){
                auxiliar_in = "ietf-schc:fid-" + input_line("\nEjemplo ipv6-version, udp-length, etc:  ");
            }else{
                auxiliar_in = "ietf-schc:fid-" + input_line(" :  ");
            }
            std::strncpy(entry.FID, auxiliar_in.c_str(), sizeof(entry.FID)-1);
            entry.FID[sizeof(entry.FID)-1] = '\0';

            auxiliar_in = input_line("|-- Ingrese Field Length:  ");
            //To verify if FL ingressed is a number or a string (for variable length functions)
            int aux = std::all_of(auxiliar_in.begin(), auxiliar_in.end(), [](unsigned char c){
                return std::isdigit(c);
            });

            if ((!auxiliar_in.empty()) && aux) { //if FL is number
                entry.FL.bit_length = static_cast<uint8_t>(std::stoul(auxiliar_in));
                entry.FL.type = "FIXED";
                
            } else {//if it is variable
                entry.FL.bit_length = 0;
                entry.FL.type = auxiliar_in;
            }
            entry.FP = static_cast<uint8_t>(std::stoul(input_line("|-- Ingrese Field Position:  ")));
            entry.DI = di_from_string(input_line("|-- Ingrese Direction Indicator:  "));
            
            entry.TV.index = static_cast<uint16_t>(std::stoul(input_line("|-- Indique cuantos items tiene la lista de Target Value:  ")));
            entry.TV.value_matrix.clear();
            entry.TV.value_matrix.reserve(entry.TV.index);

            for (uint16_t k = 0; k < entry.TV.index; ++k) {
                std::string line = input_line(
                    "|-- Ingrese el item TV #" + std::to_string(k) +
                    " como bytes hex (ej: [0xaa, 0x4e] o aa4e):  ");
                auto bytes = parse_hex_bytes(line);
                if (bytes.empty()) {
                    throw std::runtime_error("TV item vacío o inválido en k=" + std::to_string(k));
                }
                entry.TV.value_matrix.push_back(std::move(bytes));
            }

            // por seguridad, sincronizamos index con el tamaño real
            entry.TV.index = static_cast<uint16_t>(entry.TV.value_matrix.size());


            entry.MO = mo_from_string(input_line("|-- Ingrese Matching Operator:  "));
            if(entry.MO == matching_operator_t::MSB_){ //cast MO to verify if it is MSB
                entry.MsbLength = static_cast<uint8_t>(std::stoul(input_line("|-- Ingrese cantidad de bits:  ")));
                
            }
            else
            {
                entry.MsbLength = 0;
            }
            

            entry.CDA = cda_from_string(input_line("|-- Ingrese Compression Decompression Action:  "));
            entries.push_back(entry);
            spdlog::info("entry agregada {}", i);
        }
        newrule.setRule(newID, newrule_id_length, newnature, entries);
        spdlog::info("Nueva regla completada");

}
void insert_rule_into_context(RuleContext& ctx, const SCHC_Rule& rule) {
    auto [it, inserted] = ctx.emplace(rule.getRuleID(), rule); // copia
    if (!inserted) {
        spdlog::error("RuleID duplicado en memoria: {}", rule.getRuleID());
        throw std::runtime_error("RuleID duplicado en memoria: " + std::to_string(rule.getRuleID()));
        
    }
}

