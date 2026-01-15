#include "SCHC_RuleID.hpp"
#include "SCHC_RulesTest.hpp"
#include "SCHC_MACROS.hpp"
#include <cstring>  // Para strcpy

// Función para cargar reglas predefinidas
SCHC_RuleID* loadPredefinedRules(int& numRules) {
    static SCHC_RuleID rules[MAX_RULES];  // Arreglo estático de 2 reglas (ajusta el tamaño)
    numRules = 2;  // Número de reglas cargadas
    int* POINTER = &numRules; //solamente para diferenciar un puntero nulo de uno con valor
    // Regla 1
    FieldDescription fields1[4];
    strcpy(fields1[0].FID, "IPv6 Version");
    fields1[0].FL = 4;
    fields1[0].FP = 1;
    fields1[0].DI = direction_indicator_t::BI;
    fields1[0].TV = ZERO_POINTER;
    fields1[0].MO = matching_operator_t::IGNORE;
    fields1[0].MsbLength = 0;
    fields1[0].CDA = cd_action_t::NOT_SENT;

    strcpy(fields1[1].FID, "IPv6 DiffServ");
    fields1[1].FL = 8;
    fields1[1].FP = 1;
    fields1[1].DI = direction_indicator_t::BI;
    fields1[1].TV = ZERO_POINTER;
    fields1[1].MO = matching_operator_t::EQUAL;
    fields1[1].MsbLength = 0;
    fields1[1].CDA = cd_action_t::NOT_SENT;

    strcpy(fields1[2].FID, "IPv6 Flow Label");
    fields1[2].FL = 20;
    fields1[2].FP = 1;
    fields1[2].DI = direction_indicator_t::BI;
    fields1[2].TV = ZERO_POINTER;
    fields1[2].MO = matching_operator_t::EQUAL;
    fields1[2].MsbLength = 0;
    fields1[2].CDA = cd_action_t::NOT_SENT;

    strcpy(fields1[3].FID, "UDP DevPort");
    fields1[3].FL = 16;
    fields1[3].FP = 1;
    fields1[3].DI = direction_indicator_t::BI;
    fields1[3].TV = ZERO_POINTER;
    fields1[3].MO = matching_operator_t::EQUAL;
    fields1[3].MsbLength = 0;
    fields1[3].CDA = cd_action_t::NOT_SENT;


    rules[0] = SCHC_RuleID(0x01, 0x10, fields1);
    
    // Regla 2
    FieldDescription fields2[4];

    /* ================= UDP DevPort ================= */
    strcpy(fields2[0].FID, "UDP DevPort");
    fields2[0].FL = 16;
    fields2[0].FP = false;
    fields2[0].TV = POINTER;                 // 8720 ≠ 0
    fields2[0].DI = direction_indicator_t::BI;
    fields2[0].MO = matching_operator_t::MSB;
    fields2[0].MsbLength = 12;
    fields2[0].CDA = cd_action_t::LSB;

    /* ================= UDP AppPort ================= */
    strcpy(fields2[1].FID, "UDP AppPort");
    fields2[1].FL = 16;
    fields2[1].FP = false;
    fields2[1].TV = POINTER;                 // 8720 ≠ 0
    fields2[1].DI = direction_indicator_t::BI;
    fields2[1].MO = matching_operator_t::MSB;
    fields2[1].MsbLength = 12;
    fields2[1].CDA = cd_action_t::LSB;

    /* ================= UDP Length ================= */
    strcpy(fields2[2].FID, "UDP Length");
    fields2[2].FL = 16;
    fields2[2].FP = false;
    fields2[2].TV = ZERO_POINTER;             // TV = 0
    fields2[2].DI = direction_indicator_t::BI;
    fields2[2].MO = matching_operator_t::IGNORE;
    fields2[2].MsbLength = 0;
    fields2[2].CDA = cd_action_t::COMPUTE;

    /* ================= UDP Checksum ================= */
    strcpy(fields2[3].FID, "UDP Checksum");
    fields2[3].FL = 16;
    fields2[3].FP = false;
    fields2[3].TV = ZERO_POINTER;             // TV = 0
    fields2[3].DI = direction_indicator_t::BI;
    fields2[3].MO = matching_operator_t::IGNORE;
    fields2[3].MsbLength = 0;
    fields2[3].CDA = cd_action_t::COMPUTE;

    
    rules[1] = SCHC_RuleID(0x02, 0x10, fields2);
    
    return rules;  // Devuelve el puntero al arreglo
}