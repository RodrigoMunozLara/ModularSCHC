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

#include <cstdint>

#pragma once

enum class matching_operator_t : uint8_t {
    IGNORE_ = 0,
    EQUAL_ = 1,
    MSB_ = 2,
    MATCH_MAPPING_ = 3
};

enum class direction_indicator_t : uint8_t {
    UP = 0,
    DOWN = 1,
    BI = 2,
    SKIP = 3 //para cosntruir regla default o paquetes sin compresión
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

//Estructura que define cada campo de una regla SCHC

struct FieldDescription {
    char FID[32]; //Field Identifier
    uint8_t FL; //Field Length in bits
    uint32_t FP; //Field Position
    void* TV; //Target Value (pointer to pointer to void to allow any data type created in the RuleID class)
    direction_indicator_t DI; //Direction Indicator
    matching_operator_t MO; //Matching Operator
    uint8_t MsbLength; //MSB Length (only used if MO is MSB)
    cd_action_t CDA; //Compression/Decompression Action
};

//+------------------------------------------------------------------------------------------------------+
//Clase que define una regla SCHC
//Se espera múltiples instancias de esta clase para definir las reglas SCHC partiendo desde el RuleID_0
//que corresponde al caso en que no se aplica compresión alguna.

class SCHC_RuleID {
    private:
        uint32_t rule_id; //Rule ID
        uint16_t rule_id_length; //Rule ID Length in bits
        uint8_t dev_id; //Device ID
        FieldDescription* fields; //Arreglo de descripciones de campos
        //dudas si dejar el contenido de TV dentro de la clase o definirlo fuera de ella por problemas de memoria
    public:
        SCHC_RuleID(); //Default Constructor
        SCHC_RuleID(uint32_t rid, uint16_t rid_length, uint8_t dev_id, FieldDescription* flds); //Constructor
        ~SCHC_RuleID(); //Destructor

        uint32_t getRuleID(); //Getter for Rule ID
        uint8_t getDevID(); //Getter for Device ID
        uint16_t getRuleIDLength(); //Getter for Rule ID Length in bits
        FieldDescription* getFields(); //Getter for Fields

        bool setFID(uint32_t ruleNumber, uint16_t ruleIDLength, uint8_t devID, FieldDescription* flds); //Setter for RuleID
        

        void printRule() const; //Print Rule Details
};