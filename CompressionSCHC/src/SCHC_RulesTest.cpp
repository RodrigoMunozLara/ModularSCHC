#include "SCHC_RulesTest.hpp"
#include "SCHC_RuleID.hpp"
#include "SCHC_RulesPreLoad.hpp"
#include <vector>
#include <iostream>

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