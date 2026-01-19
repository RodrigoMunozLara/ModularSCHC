#include "SCHC_RuleID.hpp"
#include <iostream>
#include <cstring>

// Default Constructor
SCHC_RuleID::SCHC_RuleID() : rule_id(0), creator_id(0), fields(nullptr) {}

// Constructor
SCHC_RuleID::SCHC_RuleID(uint8_t rid, uint8_t cid, FieldDescription* flds)
    : rule_id(rid), creator_id(cid), fields(flds) {}

// Destructor
SCHC_RuleID::~SCHC_RuleID() {
    // Assuming fields is managed externally, no delete here
}

// Getter for Rule ID
uint8_t SCHC_RuleID::getRuleID() {
    return rule_id;
}

// Getter for Creator ID
uint8_t SCHC_RuleID::getCreatorID() {
    return creator_id;
}

// Getter for Fields
FieldDescription* SCHC_RuleID::getFields() {
    return fields;
}

// Setter for RuleID
bool SCHC_RuleID::setFID(uint8_t ruleNumber, uint8_t creatorNumber, FieldDescription* flds) {
    rule_id = ruleNumber;
    creator_id = creatorNumber;
    fields = flds;
    return true;
}

// Print Rule Details
void SCHC_RuleID::printRule() const {
    std::cout << "Rule ID: " << static_cast<int>(rule_id) << std::endl;
    std::cout << "Creator ID: " << static_cast<int>(creator_id) << std::endl;
    if (fields) {
        std::cout << "Fields: " << std::endl;
        // Assuming fields is an array, but need to know how many
        // For now, print first field or something, but since it's a pointer, perhaps loop until null or something.
        // The code uses fixed arrays, so perhaps print as is.
        // To keep simple, print the pointer or something.
        std::cout << "Fields pointer: " << fields << std::endl;
    }
}