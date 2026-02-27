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

#ifndef SCHC_RULE_HPP
#define SCHC_RULE_HPP

#include <cstdint>
#include <iostream>
#include <vector>
#include <iomanip>

enum class nature_type_t : uint8_t {
    COMPRESSION = 0,
    NO_COMPRESSION = 1,
    FRAGMENTATION = 2,
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
    uint16_t size; //number of items of the TV list
    std::vector<std::vector<uint8_t>> value_matrix; //matrix of vectors of bytes, to represent each of tv values
    //used instead of void* to avoid memory management issues and better type safety
};

struct FieldLenght { //to respect the YANG SCHC model
    uint8_t bit_length;
    std::string type; //to save identity-type. Example: fl-variable

};

//struct that defines the description of each field in a SCHC Rule
//represents an entry in the YANG model for SCHC Rules

struct SCHC_Entry {
    std::string FID; //Field Identifier as string
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

class SCHC_Rule{

    private:
        uint8_t rule_id; //Rule ID
        uint8_t rule_id_length; //Rule ID Length in bits
        nature_type_t nature_type; //Nature Type (Compression, No Compression, Fragmentation)
       
        std::vector<SCHC_Entry> fields; //Vector of Field Descriptions (SCHC_Entry structs)

    public:
        SCHC_Rule(); //Default Constructor

        SCHC_Rule(uint8_t rid, uint8_t rid_length, nature_type_t ntype)
            : rule_id(rid), rule_id_length(rid_length), nature_type(ntype) {}
        
        SCHC_Rule(uint8_t rid, uint8_t rid_length, nature_type_t nature, std::vector<SCHC_Entry> flds);

        ~SCHC_Rule(); //Destructor

        uint8_t getRuleID() const ;//Getter for Rule ID
        uint8_t getRuleIDLength() const ;//Getter for Rule ID Length in bits
        nature_type_t getNatureType() const ;//Getter for Nature Type

        const std::vector<SCHC_Entry>& getFields() const; //Getter for Fields vector
        void addField(const SCHC_Entry& field); //Method to add a Field Description to the Fields vector
        
        void setRule(uint8_t ruleNumber, uint8_t ruleid_length, nature_type_t nature, const std::vector<SCHC_Entry> &flds);
        void printRuleOut() const; //Method to print the Rule details


};

//Functions to print field values of the rule

std:: ostream& operator <<(std:: ostream& os, const cd_action_t & cda); // For printing with de enum values
std:: ostream & operator <<(std::ostream &os, const TV_item &tv);
#endif