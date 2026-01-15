#ifndef PREDEFINED_RULES_HPP
#define PREDEFINED_RULES_HPP

#include "SCHC_RuleId.hpp"

// Declaración de función para cargar reglas predefinidas
// Devuelve un arreglo de reglas (o usa un vector si prefieres)
SCHC_RuleID* loadPredefinedRules(int& numRules);  // numRules indica cuántas reglas se cargaron

#endif // PREDEFINED_RULES_HPP