#include "SCHC_RuleID.hpp"
#include <iostream>
#include <utility>
#include <cstring>

// Default Constructor implementation
SCHC_RuleID::SCHC_RuleID() 
    : rule_id(0), 
      rule_id_length(0),
      nature_type(nature_type_t::COMPRESSION),
      fields() {}

// Constructor
SCHC_RuleID::SCHC_RuleID(uint32_t rid, uint8_t rid_length, nature_type_t nature, std::vector<SCHC_Entry> flds) 
    : rule_id(rid), rule_id_length(rid_length), nature_type(nature),  fields(std::move(flds)) {}

// Destructor
SCHC_RuleID::~SCHC_RuleID() = default;

// Getter for Rule ID
uint32_t SCHC_RuleID::getRuleID() const {
    return rule_id;
}


// Setter for RuleID
bool SCHC_RuleID::setFID(uint32_t ruleNumber, uint8_t ruleid_length, nature_type_t nature, const std::vector<SCHC_Entry> &flds) {
    this->rule_id = ruleNumber;
    this->rule_id_length = ruleid_length;
    this->nature_type = nature;
    this->fields = flds;
    return true;
}

// Print Rule Details
void SCHC_RuleID::printRuleOut() const {
    std::cout << "Rule ID: " << rule_id << '\n';
    std::cout << "Rule ID Length: " << static_cast<unsigned>(rule_id_length) << '\n';
    std::cout << "Fields count: " << fields.size() << '\n';
    printFields(fields);
    
}

