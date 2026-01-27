#ifndef SCHC_RULES_MANAGER_HPP
#define SCHC_RULES_MANAGER_HPP

#include "SCHC_RuleID.hpp"   // Debe traer FieldDescription, direction_indicator_t, etc.
#include <cstdint>
#include <unordered_map>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>



// =====================
// Estructuras de soporte
// =====================

struct TVOwner {
    std::unique_ptr<uint64_t> num;
    std::unique_ptr<std::vector<uint64_t>> vec;
};

struct LoadedRule {
    uint32_t ruleid{};
    std::optional<std::string> devid;
    std::unique_ptr<FieldDescription[]> fields;
    size_t fieldCount{0};
    std::vector<TVOwner> tvOwners;
};

// =====================
// API pública
// =====================

std::unordered_map<uint32_t, LoadedRule> load_rules_ini(const char* path);
std::vector<SCHC_RuleID> loadPredefinedRules();
void printRules(const std::unordered_map<uint32_t, LoadedRule>& rules);
LoadedRule create_rule() ;
void writeRuleToIni(const std::string& iniPath, const LoadedRule& rule);


#endif // SCHC_RULES_PARSERINI_HPP
