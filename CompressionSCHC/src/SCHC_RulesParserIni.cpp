#define SI_SUPPORT_IOSTREAMS
#include "SimpleIni.h"
#include "SCHC_RuleID.hpp"
#include "SCHC_RulesParserIni.hpp"
#include <spdlog/spdlog.h>  

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

//Función auxiliar para eliminar expacios en blanco al inicio y final de una cadena
static inline std::string trim(std::string s) {
    auto notSpace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

//Función auxiliar para verificar si una cadena comienza con un prefijo 
static inline bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

//Función auxiliar para convertir una cadena a minúsculas
static inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}
//Función auxiliar para dividir una línea CSV simple (sin comillas ni escapes), ignorando comas dentro de [ ]
static std::vector<std::string> split_csv_simple(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    int bracketLevel = 0;  // Contador para nivel de brackets
    for (char ch : line) {
        if (ch == '[') {
            bracketLevel++;
            cur.push_back(ch);
        } else if (ch == ']') {
            bracketLevel--;
            cur.push_back(ch);
        } else if (ch == ',' && bracketLevel == 0) {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    out.push_back(trim(cur));
    return out;
}

//Función auxiliar para parsear un uint64_t desde cadena
static uint64_t parse_u64(const std::string& s) {
    std::string t = trim(s);
    if (t.empty()) throw std::runtime_error("numero vacio");
    size_t idx = 0;
    int base = (starts_with(t, "0x") || starts_with(t, "0X")) ? 16 : 10;
    uint64_t v = std::stoull(t, &idx, base);
    if (idx != t.size()) throw std::runtime_error("numero invalido: " + t);
    return v;
}

//Función auxiliar para verificar si una cadena tiene sintaxis de lista [v1,v2,...] para TV
static bool is_list_syntax(const std::string& s) {
    std::string t = trim(s);
    return t.size() >= 2 && t.front() == '[' && t.back() == ']';
}

//Función auxiliar para parsear una lista de uint64_t desde cadena con sintaxis [v1,v2,...]
static std::vector<uint64_t> parse_u64_list(const std::string& s) {
    std::string t = trim(s);
    if (!is_list_syntax(t)) throw std::runtime_error("TV no es lista: " + t);
    t = trim(t.substr(1, t.size() - 2)); // inside [...]
    if (t.empty()) return {};
    auto parts = split_csv_simple(t);
    std::vector<uint64_t> vals;
    vals.reserve(parts.size());
    for (auto& p : parts) vals.push_back(parse_u64(p));
    return vals;
}

//Función auxiliar para copiar FID a FieldDescription
static void copy_FID(char dest[32], const std::string& fid) {
    std::memset(dest, 0, 32);
    std::strncpy(dest, fid.c_str(), 31); // 31 + '\0'
}


//+--------------------------------------------------------------+
//  Funciones auxiliares para formatear los enums desde cadena
static direction_indicator_t parse_DI(const std::string& s) {
    std::string t = lower(trim(s));
    if (t == "up") return direction_indicator_t::UP;
    if (t == "down") return direction_indicator_t::DOWN;
    if (t == "bi") return direction_indicator_t::BI;
    throw std::runtime_error("DI desconocido: " + s);
}

static matching_operator_t parse_MO(const std::string& s, uint8_t& msbLenOut) {
    msbLenOut = 0;
    std::string t = lower(trim(s));

    if (t == "equal") return matching_operator_t::EQUAL_;
    if (t == "ignore") return matching_operator_t::IGNORE_;
    if (t == "match-mapping") return matching_operator_t::MATCH_MAPPING_;

    // "MSB" o "MSB(x)"
    if (starts_with(t, "msb")) {
        auto p1 = t.find('(');
        auto p2 = t.find(')');
        if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1 + 1) {
            // extraer valor entre paréntesis y lo guarda en msbLenOut
            msbLenOut = static_cast<uint8_t>(parse_u64(t.substr(p1 + 1, p2 - p1 - 1)));
        } else {
            msbLenOut = 0; // si viene "MSB" sin paréntesis
        }
        return matching_operator_t::MSB_;
    }

    throw std::runtime_error("MO desconocido: " + s);
}


static cd_action_t parse_CDA(const std::string& s) {
    std::string t = lower(trim(s));
    if (t == "not-sent") return cd_action_t::NOT_SENT;
    if (t == "value-sent") return cd_action_t::VALUE_SENT;
    if (t == "mapping-sent") return cd_action_t::MAPPING_SENT;
    if (t == "lsb") return cd_action_t::LSB;

    if (t == "compute" || t == "compute-length" || t == "compute-checksum") return cd_action_t::COMPUTE;

    if (t == "dev-idd") return cd_action_t::DEV_IDD;
    if (t == "app-idd") return cd_action_t::APP_IDD;

    throw std::runtime_error("CDA desconocido: " + s);
}
//+--------------------------------------------------------------+

//Función para parsear una línea de field y devolver FieldDescription
static FieldDescription parse_field_line(const std::string& rhs, TVOwner& ownerOut) {
    spdlog::info("Parsing field line: {}", rhs);  // Agrega logging aquí
    // rhs: FID,FL,FP,DI,TV,MO,CDA
    auto cols = split_csv_simple(rhs);
    if (cols.size() == 1 && (cols[0].empty()||cols[0] == "0")) {
        FieldDescription f{};
        f.FID[0] = '\0';
        f.FL = 0;
        f.FP = 0;
        f.DI = direction_indicator_t::SKIP;
        f.MO = matching_operator_t::IGNORE_;
        f.CDA = cd_action_t::VALUE_SENT;
        return f;
    }
    else if (cols.size() != 7) throw std::runtime_error("Se esperaban 7 columnas en: " + rhs);

    FieldDescription f{};
    copy_FID(f.FID, cols[0]);

    f.FL = static_cast<uint8_t>(parse_u64(cols[1]));     // bits
    f.FP = (parse_u64(cols[2]) != 0);
    f.DI = parse_DI(cols[3]);

    f.MO = parse_MO(cols[5], f.MsbLength);
    f.CDA = parse_CDA(cols[6]);

    std::string tv = trim(cols[4]);
    std::string tvL = lower(tv);

    if (tvL == "none") {
        f.TV = nullptr;
        ownerOut.num = nullptr;  // No asignar num para NONE
    } else if (f.MO == matching_operator_t::MATCH_MAPPING_) {
        // TV lista
        auto list = parse_u64_list(tv);
        ownerOut.vec = std::make_unique<std::vector<uint64_t>>(std::move(list));
        f.TV = ownerOut.vec.get();
    } else {
        // TV escalar
        ownerOut.num = std::make_unique<uint64_t>(parse_u64(tv));
        f.TV = ownerOut.num.get();
    }

    return f;
}

//Función principal para cargar reglas SCHC desde archivo INI
std::map<std::string, LoadedRule> load_rules_ini(const char* path) {
    CSimpleIniA ini;
    ini.SetUnicode(true);
    ini.SetMultiKey(false);
    ini.SetMultiLine(true);

    SI_Error rc = ini.LoadFile(path);
    if (rc < 0) throw std::runtime_error("No pude cargar el INI");

    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);

    std::map<std::string, LoadedRule> out;

    for (const auto& s : sections) {
        std::string sec = s.pItem;

        LoadedRule rule;
        rule.ruleid = static_cast<uint32_t>(parse_u64(ini.GetValue(sec.c_str(), "ruleid", "0")));

        // devid opcional
        std::string dev = ini.GetValue(sec.c_str(), "devid", "NONE");
        if (lower(trim(dev)) == "none") rule.devid = std::nullopt;
        else rule.devid = dev;

        // Keys de la sección
        CSimpleIniA::TNamesDepend keys;
        ini.GetAllKeys(sec.c_str(), keys);

        // f00,f01,... ordenadas
        std::vector<std::string> fkeys;
        for (const auto& k : keys) {
            std::string key = k.pItem;
            if (starts_with(key, "f")) fkeys.push_back(key);
        }
        std::sort(fkeys.begin(), fkeys.end());

        rule.fieldCount = fkeys.size();
        rule.fields = std::make_unique<FieldDescription[]>(rule.fieldCount);
        rule.tvOwners.resize(rule.fieldCount); // 1 owner por field

        for (size_t i = 0; i < fkeys.size(); ++i) {
            const char* v = ini.GetValue(sec.c_str(), fkeys[i].c_str(), "");
            if (!v || !*v) throw std::runtime_error("Campo vacio en " + sec + ":" + fkeys[i]);

            rule.fields[i] = parse_field_line(v, rule.tvOwners[i]);
        }

        out[sec] = std::move(rule);
    }

    return out;
}


std::vector<SCHC_RuleID> loadPredefinedRules() {
    std::vector<SCHC_RuleID> rules;
    try {
        auto loaded = load_rules_ini("../config/RulesPreLoad.ini");
        for (const auto& [name, lr] : loaded) {
            uint8_t dev_id = 0; // default
            if (lr.devid) {
                try {
                    dev_id = static_cast<uint8_t>(std::stoi(*lr.devid));
                } catch (...) {
                    dev_id = 0;
                }
            }
            uint16_t rid_length = 8; // assume 8 bits for rule ID length
            SCHC_RuleID rule(lr.ruleid, rid_length, dev_id, lr.fields.get());
            rules.push_back(rule);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading rules from INI: " << e.what() << std::endl;
        // Fallback to dummy rule
        FieldDescription* fields = nullptr;
        rules.push_back(SCHC_RuleID(1, 8, 1, fields));
    }
    return rules;
}
//Función auxiliar para imprimir reglas cargadas (para depuración)
void printRules(const std::map<std::string, LoadedRule>& rules) {
    std::cout << "Número de reglas cargadas: " << rules.size() << std::endl;
    for (const auto& pair : rules) {
        const std::string& name = pair.first;
        const LoadedRule& rule = pair.second;
        std::cout << "Regla: " << name << " (ID: " << rule.ruleid << ")" << std::endl;
        if (rule.devid) {
            std::cout << "  DevID: " << *rule.devid << std::endl;
        } else {
            std::cout << "  DevID: NONE" << std::endl;
        }
        std::cout << "  Campos (" << rule.fieldCount << "):" << std::endl;
        for (size_t i = 0; i < rule.fieldCount; ++i) {
            const FieldDescription& f = rule.fields[i];
            std::cout << "    " << f.FID << " - FL: " << (int)f.FL << ", FP: " << f.FP << ", DI: " << (int)f.DI << ", MO: " << (int)f.MO << ", CDA: " << (int)f.CDA << std::endl;
        }
        std::cout << std::endl;
    }
}
