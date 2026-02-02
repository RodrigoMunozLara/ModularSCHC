/*
Static Context Header File
This file defines the StaticContext in de SCHC protocol, 
which is used for compressing and decompressing headers in constrained networks.
It contains the definition of the RuleID class and the FieldDescription struct for it.
Also contains enum parameters for the Rules created.
*/

//+------------------------------------------------------------------------------------------------------+
//Enum values para simplificar la lectura de las reglas. Reemplazan los valores numéricos en las reglas.
//Se optimiza el uso de memoria usando uint8_t en lugar de int

#ifndef SCHC_RULEID.HPP
#define SCHC_RULEID.HPP

#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>
#pragma once

enum class nature_type_t : uint8_t {
    COMPRESSION = 0,
    NO_COMPRESSION = 1,
    FRAGMENTATION = 2,
    NONE = 3 //Just for creating an empty rule
};

enum class matching_operator_t : uint8_t {
    IGNORE_ = 0,
    EQUAL_ = 1,
    MATCH_MAPPING_ = 2,
    MSB_ = 3
};

enum class direction_indicator_t : uint8_t {
    UP = 0,
    DOWN = 1,
    BI = 2
};

enum class cd_action_t : uint8_t {
    NOT_SENT = 0,
    VALUE_SENT = 1,
    MAPPING_SENT = 2,
    LSB = 3,
    COMPUTE = 4,
    DEV_IDD = 5,
    APP_IDD = 6
};


//+------------------------------------------------------------------------------------------------------+

//struct container of TV index and value:
struct TV_item
{
    uint16_t index; //index of the TV in the list of TVs of the field
    std::vector<uint8_t> value; //value of the TV
    //used instead of void* to avoid memory management issues and better type safety
};

struct FieldLenght { //to respect the YANG SCHC model
    uint8_t bit_length;
    std::string type; //to save identity-type. Example: fl-variable

};

//struct that defines the description of each field in a SCHC Rule
//represents an entry in the YANG model for SCHC Rules

struct SCHC_Entry {
    char FID[64]; //Field Identifier as string
    FieldLenght FL;
    TV_item TV;
    uint8_t FP; //Field Position
    direction_indicator_t DI; //Direction Indicator
    matching_operator_t MO; //Matching Operator
    uint8_t MsbLength; //MSB Length (only used if MO is MSB)
    cd_action_t CDA; //Compression/Decompression Action
};

//+------------------------------------------------------------------------------------------------------+

//Class that models a SCHC Rule

class SCHC_RuleID {

    private:
        uint32_t rule_id; //Rule ID
        uint8_t rule_id_length; //Rule ID Length in bits
        nature_type_t nature_type; //Nature Type (Compression, No Compression, Fragmentation)
       
        std::vector<SCHC_Entry> fields; //Vector of Field Descriptions (SCHC_Entry structs)

    public:
        SCHC_RuleID(); //Default Constructor

        SCHC_RuleID(uint32_t rid, uint16_t rid_length, nature_type_t ntype)
            : rule_id(rid), rule_id_length(rid_length), nature_type(ntype) {}
        
        SCHC_RuleID(uint32_t rid, uint8_t rid_length, nature_type_t nature, std::vector<SCHC_Entry> flds);

        ~SCHC_RuleID(); //Destructor

        uint32_t getRuleID() const {return rule_id;} //Getter for Rule ID
        uint8_t getRuleIDLength() const {return rule_id_length;} //Getter for Rule ID Length in bits
        nature_type_t getNatureType() const {return nature_type;} //Getter for Nature Type

        const std::vector<SCHC_Entry>& getFields() const {return fields;} //Getter for Fields vector
        std::vector<SCHC_Entry>& getFields() {return fields;} //Non-const Getter for Fields vector

        bool setFID(uint32_t ruleNumber, uint8_t ruleid_length, nature_type_t nature, const std::vector<SCHC_Entry> &flds);
        
        void addField(const SCHC_Entry& field); //Method to add a Field Description to the Fields vector

        void printRuleOut() const; //Method to print the Rule details


};

//Functions for printing the fields:

std:: ostream& operator <<(std:: ostream& os, const cd_action_t & cda){ // For printing with de enum values
    return os << static_cast<int>(cda);
}

std:: ostream & operator <<(std::ostream &os, const TV_item &tv){
    if (tv.value.empty()){ 
        return os << "null";
    }
    os << "0x";
    for (uint8_t b : tv.value) {// prints each byte in hex with two nibbles
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    os << std::dec;
    return os;

}
static void printEntry(const SCHC_Entry& e) {
    std::string mo_fix;
    if(e.MO== matching_operator_t::MSB_){
        mo_fix = "MSB(" + std::to_string(e.MsbLength) + ")";
    }else{
        mo_fix = std::to_string(e.MsbLength);
    }
    
    if(e.TV.index){
        

    }

    std::cout << "  [" << e.FID
              << " | FL size=" << e.FL.bit_length
              << "; FL type=" << e.FL.type
              << " | FP="<< e.FP
              << "TV=" << e.TV
              << " |" << mo_fix
              << " | CDA=" << e.CDA
              << "]\n";
}

static void printFields(const std::vector<SCHC_Entry>& fields) {
    std::cout << "Fields (" << fields.size() << "):\n";
    for (const auto& f : fields) {
        printEntry(f);
    }
}

#endif