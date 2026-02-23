#include "SCHC_Rule.hpp"
#include "SCHC_RulesManager.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <utility>
#include <cstring>

// Default Constructor implementation
SCHC_Rule::SCHC_Rule() 
    : rule_id(0), 
      rule_id_length(0),
      nature_type(nature_type_t::COMPRESSION),
      fields() {}

// Constructor
SCHC_Rule::SCHC_Rule(uint32_t rid, uint8_t rid_length, nature_type_t nature, std::vector<SCHC_Entry> flds) 
    : rule_id(rid), rule_id_length(rid_length), nature_type(nature),  fields(std::move(flds)) {}

// Destructor
SCHC_Rule::~SCHC_Rule() = default;

// Getter for Rule ID
uint32_t SCHC_Rule::getRuleID() const {
    return rule_id;
}
// Getter for Rule length
uint8_t SCHC_Rule::getRuleIDLength() const {
    return rule_id_length;
}

nature_type_t SCHC_Rule::getNatureType() const {
    return nature_type;
}

const std::vector<SCHC_Entry>& SCHC_Rule::getFields() const {
    return fields; 
}

void SCHC_Rule::addField(const SCHC_Entry &entry){
    fields.push_back(entry);
}



// Setter for RuleID
void SCHC_Rule::setRule(uint32_t ruleNumber, uint8_t ruleid_length, nature_type_t nature, const std::vector<SCHC_Entry> &flds) {
    this->rule_id = ruleNumber;
    this->rule_id_length = ruleid_length;
    this->nature_type = nature;
    this->fields = flds;
}

//Functions and helpers to print Rules:


//MO to string for print
static std::string mo_to_string(const SCHC_Entry& e) {
    switch (e.MO) {
        case matching_operator_t::EQUAL_:
            return "equal";
        case matching_operator_t::IGNORE_:
            return "ignore";
        case matching_operator_t::MATCH_MAPPING_:
            return "match-mapping";
        case matching_operator_t::MSB_:
            return "MSB(" + std::to_string(e.MsbLength) + ")";
        default:
            return "MO(" + std::to_string(static_cast<int>(e.MO)) + ")";
    }
}

// Convierte CDA a string
static std::string cda_to_string(cd_action_t cda) {
    switch (cda) {
        case cd_action_t::NOT_SENT:
            return "not-sent";
        case cd_action_t::VALUE_SENT:
            return "value-sent";
        case cd_action_t::MAPPING_SENT:
            return "mapping-sent";
        case cd_action_t::LSB:
            return "LSB";
        case cd_action_t::COMPUTE:
            return "compute";
        case cd_action_t::DEV_IDD:
            return "DEV_IDD";
        case cd_action_t::APP_IDD:
            return "APP_IDD";
        default:
            return "CDA(" + std::to_string(static_cast<int>(cda)) + ")";
    }
}

static std::string mo_to_string(matching_operator_t mo, uint8_t msb_len) {
    switch (mo) {
        case matching_operator_t::EQUAL_:
            return "equal";
        case matching_operator_t::IGNORE_:
            return "ignore";
        case matching_operator_t::MATCH_MAPPING_:
            return "match-mapping";
        case matching_operator_t::MSB_:
            return "MSB(" + std::to_string(msb_len) + ")";
        default:
            return "MO(" + std::to_string(static_cast<int>(mo)) + ")";
    }
}




std:: ostream& operator <<(std:: ostream& os, const cd_action_t & cda){ // For printing with de enum values
    return os << cda_to_string(cda);
}
std::ostream& operator<<(std::ostream& os, const TV_item& tv) {
    // Si no hay TV
    if (tv.size== 0 || tv.value_matrix.empty()) {
        return os << "[]";
    }

    //Clase base para stream de salida
    std::ios old_state(nullptr);
    //Copiar el formato de stream de salida antes de sobrecargarlo
    old_state.copyfmt(os);

    os << "[";
    os << std::hex << std::setfill('0');

    const size_t n = std::min<size_t>(tv.size, tv.value_matrix.size());

    for(size_t i = 0; i<n ; i++){
        const auto &item = tv.value_matrix[i];
        os <<"[";
        for (size_t j = 0; j < item.size(); j++)
        {
            os<<"0x"<< std::setw(2) <<static_cast<int>(item[j]);
            if(j+1 <item.size()) {
                os << ", ";
            };
        }
        os << "]";
        if(i+1 <n){
            os<<", ";
        }
    }
    os << "]";
    //restrablecer el formato de stream original
    os.copyfmt(old_state);
    return os;

}



static void printEntry(const SCHC_Entry& e) {
    std::cout << "  [ " << e.FID
              << " | FL size = " << static_cast<int>(e.FL.bit_length)
              << " ; FL type = " << e.FL.type
              << " | FP = "<< static_cast<int>(e.FP)
              << " | TV = " <<e.TV
              << " | MO = " << mo_to_string(e.MO, e.MsbLength)
              << " | CDA = " << e.CDA
              << "]\n";
}

static void printFields(const std::vector<SCHC_Entry>& fields) {
    std::cout << "Fields (" << fields.size() << "):\n";
    for (const auto& f : fields) {
        printEntry(f);
    }
}
// Print Rule Details
void SCHC_Rule::printRuleOut() const {
    std::cout << "Rule ID: " << rule_id << '\n';
    std::cout << "Rule ID Length: " << static_cast<unsigned>(rule_id_length) << '\n';
    std::cout << "Fields count: " << fields.size() << '\n';
    printFields(fields);
    
}
