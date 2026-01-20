#ifndef SCHC_RULES_PRELOAD_HPP
#define SCHC_RULES_PRELOAD_HPP

#include "SCHC_RuleID.hpp"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <optional>

// Forward declarations or includes as needed
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

std::map<std::string, LoadedRule> load_rules_ini(const char* path);

#endif // SCHC_RULES_PRELOAD_HPP