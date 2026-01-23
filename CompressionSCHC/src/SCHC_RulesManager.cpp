#define SI_SUPPORT_IOSTREAMS
#include "SimpleIni.h"
#include "SCHC_RuleID.hpp"
#include "SCHC_RulesManager.hpp"
#include <spdlog/spdlog.h>  
#include <unordered_map>
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
#include <sstream>

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
    if (!is_list_syntax(t)) {
        
        throw std::runtime_error("TV no es lista: " + t);
    }
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
    if (t == "skip") return direction_indicator_t::SKIP;
    std::cout << "DI desconocido: " << s << "\n";
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
    std::cout << "CDA desconocido: " << s << "\n";
    throw std::runtime_error("CDA desconocido: " + s);
}
//+--------------------------------------------------------------+

//Función para parsear una línea de field y devolver FieldDescription
static FieldDescription parse_field_line(const std::string& rhs, TVOwner& ownerOut) {
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
std::unordered_map<uint32_t, LoadedRule> load_rules_ini(const char* path) {
    CSimpleIniA ini;
    ini.SetUnicode(true);
    ini.SetMultiKey(false);
    ini.SetMultiLine(true);

    SI_Error rc = ini.LoadFile(path);
    if (rc < 0) throw std::runtime_error("No pude cargar el INI");

    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);

    std::unordered_map<uint32_t, LoadedRule> out;
    out.reserve(sections.size()); // reduce rehash si hay muchas

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
        fkeys.reserve(keys.size());
        for (const auto& k : keys) {
            std::string key = k.pItem;
            if (starts_with(key, "f")) fkeys.push_back(key);
        }
        std::sort(fkeys.begin(), fkeys.end());

        rule.fieldCount = fkeys.size();
        rule.fields = std::make_unique<FieldDescription[]>(rule.fieldCount);
        rule.tvOwners.resize(rule.fieldCount);

        for (size_t i = 0; i < fkeys.size(); ++i) {
            const char* v = ini.GetValue(sec.c_str(), fkeys[i].c_str(), "");
            if (!v || !*v) throw std::runtime_error("Campo vacio en " + sec + ":" + fkeys[i]);

            rule.fields[i] = parse_field_line(v, rule.tvOwners[i]);
        }

        // Key = ruleid
        uint32_t id = rule.ruleid;
        if (id == 0) {
            spdlog::warn("Seccion '{}' tiene ruleid=0 (puede ser error).", sec);
        }

        // Detectar duplicados (dos secciones con mismo ruleid)
        auto [it, inserted] = out.emplace(id, std::move(rule));
        if (!inserted) {
            throw std::runtime_error("ruleid duplicado: " + std::to_string(id) +
                                     " (seccion conflictiva: " + sec + ")");
        }

        spdlog::debug("Cargada regla id={} desde seccion '{}'", id, sec);
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
void printRules(const std::unordered_map<uint32_t, LoadedRule>& rules) {
    std::cout << "Numero de reglas cargadas: " << rules.size() << "\n";
    std::vector<uint32_t> ruleIDs;
    ruleIDs.reserve(rules.size());
    
    for (const auto& [id, _] : rules) {
        ruleIDs.push_back(id);
    }
    std::sort(ruleIDs.begin(), ruleIDs.end());
    
    for (uint32_t id : ruleIDs) {
       const LoadedRule& rule = rules.at(id);
        std::cout << "Regla ID: " << id << "\n";
        if (rule.devid) std::cout << "  DevID: " << *rule.devid << "\n";
        else           std::cout << "  DevID: NONE\n";

        std::cout << "  Campos (" << rule.fieldCount << "):\n";
        for (size_t i = 0; i < rule.fieldCount; ++i) {
            const FieldDescription& f = rule.fields[i];
            std::cout << "    " << f.FID
                      << " - FL: " << (int)f.FL
                      << ", FP: " << f.FP
                      << ", DI: " << (int)f.DI
                      << ", MO: " << (int)f.MO
                      << ", CDA: " << (int)f.CDA
                      << "\n";
        }
        std::cout << "\n";
    }
}

// Funciones auxiliares para entrada interactiva
static std::string read_line(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}
//
static size_t new_strnlen(const char* s, size_t maxlen) {
    if (!s) return 0;
    const void* p = std::memchr(s, '\0', maxlen);
    return p ? static_cast<const char*>(p) - s : maxlen;
}


//Función auxiliar para leer hex desde consola
static uint64_t read_u64(const std::string& prompt) {
    while (true) {
        try {
            return parse_u64(read_line(prompt));
        } catch (const std::exception& e) {
            std::cout << "Valor invalido. Ej: 123 o 0x1A. Error: " << e.what() << "\n";
        }
    }
}
//
static FieldDescription fid_Creator(size_t idx, TVOwner& ownerOut) {
    std::cout << "\n--- Field #" << idx << " ---\n";

    FieldDescription f{};
    ownerOut.num.reset();
    ownerOut.vec.reset();

    // FID
    while (true) {
        std::string fid = trim(read_line("FID (ej UDP.SrcPort): "));
        if (!fid.empty()) { copy_FID(f.FID, fid); break; }
        std::cout << "FID no puede ser vacio.\n";
    }

    f.FL = static_cast<uint8_t>(read_u64("FL (bits): "));
    
    while (true) {
        std::string s = lower(trim(read_line("FP (0/1): ")));
        if (s == "0" || s == "1") {
            f.FP = (s == "1");
            break;
        }
        std::cout << "FP invalido. Use 0 o 1.\n";
    }
    while (true) {
        std::string s = lower(trim(read_line("DI (up/down/bi/skip): ")));
        f.DI = parse_DI(s);
        break;
    }
    while (true) {
        std::string s = lower(trim(read_line("MO (equal/ignore/msb(x)/match-mapping):\n Si es MSB(x), reemplace x por numero de bits\n")));
        f.MO = parse_MO(s, f.MsbLength);
        break;
    }
    while (true)
    {
        std::string s = lower(trim(read_line("CDA (not-sent/value-sent/mapping-sent/lsb/compute/dev-idd/app-idd): ")));
        f.CDA = parse_CDA(s);
        break;
    }

    // TV
    if (f.MO == matching_operator_t::MATCH_MAPPING_) {
        // lista obligatoria
        while (true)
        {
            std::string s = lower(trim(read_line("Lista para TV:\n TV ejemplo: [1,2,3] o (1,2,3): ")));
            if (!(s.empty())) {
            auto vals = parse_u64_list(s);
            ownerOut.vec = std::make_unique<std::vector<uint64_t>>(std::move(vals));
            f.TV = ownerOut.vec.get();
            break;
            }   
            std::cout << "TV lista no puede ser vacia.\n"; 
        }
    } else {
        while (true) {
            std::string tv = lower(trim(read_line("TV (none o numero): ")));
            if (tv == "none") { f.TV = nullptr; break; }
            try {
                uint64_t v = parse_u64(tv);
                ownerOut.num = std::make_unique<uint64_t>(v);
                f.TV = ownerOut.num.get();
                break;
            } catch (...) {
                std::cout << "TV invalido. Usa 'none' o un numero.\n";
            }
        }
    }

    return f;
}

//Funcion para crear una nueva regla desde entrada estándar
LoadedRule create_rule() {
    LoadedRule rule;
    rule.ruleid = static_cast<uint32_t>(read_u64("RuleID (uint32): "));

    {
        std::string dev = trim(read_line("DevID (none o valor): "));
        if (!dev.empty() && lower(dev) != "none") rule.devid = dev;
        else rule.devid = std::nullopt;
    }

    size_t n = 0;
    while (n == 0) {
        n = static_cast<size_t>(read_u64("Cantidad de fields (>=1): "));
        if (n == 0) std::cout << "Debe ser >= 1.\n";
    }

    rule.fieldCount = n;
    rule.fields = std::make_unique<FieldDescription[]>(n);
    rule.tvOwners.resize(n);

    for (size_t i = 0; i < n; ++i) {
        rule.fields[i] = fid_Creator(i, rule.tvOwners[i]);
    }

    spdlog::debug("Regla creada : ruleid={} fields={}", rule.ruleid, rule.fieldCount);
    return rule;
}
//Función para escribir una regla LoadedRule a un archivo INI
void writeRuleToIni(const std::string& iniPath, const LoadedRule& rule) {
    spdlog::debug("iniciando simpleini ");
    CSimpleIniA ini;
    ini.SetUnicode(true);
    ini.SetMultiKey(false);
    ini.SetMultiLine(true);

    // Si no existe, igual luego SaveFile lo crea
    ini.LoadFile(iniPath.c_str());
    spdlog::debug("cargando archivo ini");
    const uint32_t id = rule.ruleid;
    const std::string section = "rule_" + std::to_string(id);
    spdlog::debug("escribiendo sección {}", section);

    // Limpia sección anterior para que no queden fXX viejos si cambió el tamaño
    ini.Delete(section.c_str(), nullptr);

    // ruleid / devid
    const std::string ruleidStr = std::to_string(rule.ruleid);
    ini.SetValue(section.c_str(), "ruleid", ruleidStr.c_str());

    const std::string devidStr = rule.devid ? *rule.devid : "NONE";
    ini.SetValue(section.c_str(), "devid", devidStr.c_str());

    auto di_to_str = [](direction_indicator_t di) -> const char* {
        spdlog::debug("Convirtiendo DI a string");
        switch (di) {
            case direction_indicator_t::UP:   return "UP";
            case direction_indicator_t::DOWN: return "DOWN";
            case direction_indicator_t::BI:   return "BI";
            default: return "SKIP";
        }
    };
    
    auto mo_to_str = [](matching_operator_t mo, uint8_t msbLen) -> std::string {
        spdlog::debug("Convirtiendo MO a string");
        switch (mo) {
            case matching_operator_t::EQUAL_:         return "EQUAL";
            case matching_operator_t::IGNORE_:        return "IGNORE";
            case matching_operator_t::MATCH_MAPPING_: return "MATCH-MAPPING";
            case matching_operator_t::MSB_:
                return (msbLen == 0) ? "MSB" : ("MSB(" + std::to_string(msbLen) + ")");
            default:
                return "IGNORE";
        }
    };

    auto cda_to_str = [](cd_action_t cda) -> const char* {
        spdlog::debug("Convirtiendo CDA a string");
        switch (cda) {
            case cd_action_t::NOT_SENT:     return "NOT-SENT";
            case cd_action_t::VALUE_SENT:   return "VALUE-SENT";
            case cd_action_t::MAPPING_SENT: return "MAPPING-SENT";
            case cd_action_t::LSB:          return "LSB";
            case cd_action_t::COMPUTE:      return "COMPUTE";
            case cd_action_t::DEV_IDD:      return "DEV-IDD";
            case cd_action_t::APP_IDD:      return "APP-IDD";
            default: return "VALUE-SENT";
        }
    };

    auto tv_to_str = [](const FieldDescription& f) -> std::string {
    spdlog::debug("TV: MO={} TV_ptr={}", (int)f.MO, (const void*)f.TV);
        if (f.TV == nullptr) return "NONE";

        if (f.MO == matching_operator_t::MATCH_MAPPING_) {
            const auto* vec = static_cast<const std::vector<uint64_t>*>(f.TV);
        // chequeo mínimo anti-crash (no garantiza, pero ayuda a detectar)
            if (!vec) return "NONE";
            std::ostringstream oss;
            oss << "[";
            for (size_t i = 0; i < vec->size(); ++i) {
                oss << (*vec)[i];
                if (i + 1 < vec->size()) oss << ",";
            }
            oss << "]";
            return oss.str();
        }

        const auto* num = static_cast<const uint64_t*>(f.TV);
        if (!num) return "NONE";
        return std::to_string(*num);
    };

    spdlog::debug("Definió cantidad de campos: {}", rule.fieldCount);
    // fields f00, f01, ...
    for (size_t i = 0; i < rule.fieldCount; ++i) {
        if (!rule.fields) throw std::runtime_error("rule.fields == nullptr");
        if (rule.fieldCount > 2000) throw std::runtime_error("fieldCount sospechoso");

        const FieldDescription& f = rule.fields[i];
        spdlog::debug("se creó campo vacío {}", i);
        // arma "FID,FL,FP,DI,TV,MO,CDA"
        std::ostringstream line;
        spdlog::info("DBG FID='{}' MO={} TV_ptr={}",
            std::string(f.FID, new_strnlen(f.FID, 32)),
            (int)f.MO,
            (const void*)f.TV );

        line << std::string(f.FID, new_strnlen(f.FID, 32)) << ","
             << unsigned(f.FL) << ","
             << (f.FP ? 1 : 0) << ","
             << di_to_str(f.DI) << ","
             << tv_to_str(f) << ","
             << mo_to_str(f.MO, f.MsbLength) << ","
             << cda_to_str(f.CDA);
        spdlog::debug("Escribió field {}: {}", i, line.str());
        char key[16];
        std::snprintf(key, sizeof(key), "f%02zu", i);
        ini.SetValue(section.c_str(), key, line.str().c_str());
    }
    spdlog::debug("Copió los campos desde el FieldDescription");
    SI_Error rc = ini.SaveFile(iniPath.c_str());
    spdlog::debug("Guardando archivo INI en {}", iniPath);
    if (rc < 0) throw std::runtime_error("No pude guardar INI: " + iniPath);

    spdlog::debug("writeRuleToIni(): guardada [{}] en {}", section, iniPath);
}