#ifndef SCHC_RULES_PARSERINI_HPP
#define SCHC_RULES_PARSERINI_HPP

#include "SCHC_RuleID.hpp"   // Debe traer FieldDescription, direction_indicator_t, etc.
#include <cstdint>
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

std::map<std::string, LoadedRule> load_rules_ini(const char* path);
std::vector<SCHC_RuleID> loadPredefinedRules();

// =====================
// Helpers: Funciones auxiliares para parsear
// =====================

std::map<std::string, LoadedRule> load_rules_ini(const char* path);
std::vector<SCHC_RuleID> loadPredefinedRules();
void printRules(const std::map<std::string, LoadedRule>& rules);

#endif // SCHC_RULES_PARSERINI_HPP
