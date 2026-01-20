#include "SCHC_RuleID.hpp"
#include <iostream>
#include <cstring>

// Default Constructor implementation
SCHC_RuleID::SCHC_RuleID() : rule_id(0), rule_id_length(0), dev_id(0), fields(nullptr) {}

// Constructor
SCHC_RuleID::SCHC_RuleID(uint32_t rid, uint16_t rid_length, uint8_t dev_id, FieldDescription* flds) 
    : rule_id(rid), rule_id_length(rid_length), dev_id(dev_id), fields(flds) {}

// Destructor
SCHC_RuleID::~SCHC_RuleID() {
    // Assuming fields is managed externally, no delete here
}

// Getter for Rule ID
uint32_t SCHC_RuleID::getRuleID() {
    return rule_id;
}
// Getter for Device ID
uint8_t SCHC_RuleID::getDevID() {
    return dev_id;
}
// Getter for Fields
FieldDescription* SCHC_RuleID::getFields() {
    return fields;
}

// Setter for RuleID
bool SCHC_RuleID::setFID(uint32_t ruleNumber, uint16_t ruleNumberLength, uint8_t devID, FieldDescription* flds) {
    rule_id = ruleNumber;
    rule_id_length = ruleNumberLength;
    dev_id = devID;
    fields = flds;
    return true;
}

// Print Rule Details
void SCHC_RuleID::printRule() const {
    std::cout << "Rule ID: " << static_cast<int>(rule_id) << std::endl;
    std::cout << "Rule ID Length: " << static_cast<int>(rule_id_length) << std::endl;
    std::cout << "Device ID: " << static_cast<int>(dev_id) << std::endl;
    if (fields) {
        std::cout << "Fields: " << std::endl;
        // Assuming fields is an array, but need to know how many
        // For now, print first field or something, but since it's a pointer, perhaps loop until null or something.
        // The code uses fixed arrays, so perhaps print as is.
        // To keep simple, print the pointer or something.
        std::cout << "Fields pointer: " << fields << std::endl;
    }
}