#include <iostream>
#include <vector>
#include "SCHC_Packet.hpp"
#include "SCHC_RuleID.hpp"
#include "SCHC_RulesTest.hpp"

void printRules(const std::vector<SCHC_RuleID>& rules) {
    std::cout << "Número de reglas cargadas: " << rules.size() << std::endl;
    for(size_t i = 0; i < rules.size(); i++){
        std::cout << "Regla " << i+1 << ":" << std::endl;
        rules[i].printRule();
        std::cout << std::endl;
    }
}

void addRule(std::vector<SCHC_RuleID>& rules) {
    uint8_t ruleID, creatorID;
    std::cout << "Ingrese Rule ID (uint8_t): ";
    std::cin >> ruleID;
    std::cout << "Ingrese Creator ID (uint8_t): ";
    std::cin >> creatorID;
    // Para simplicidad, crear fields vacío o pedir más, pero por ahora vacío.
    FieldDescription* fields = nullptr; // O crear uno básico
    rules.push_back(SCHC_RuleID(ruleID, creatorID, fields));
    std::cout << "Regla agregada." << std::endl;
}

int main(){
    std::vector<SCHC_RuleID> rules = loadPredefinedRules();
    
    int choice;
    do {
        std::cout << "\nMenu:" << std::endl;
        std::cout << "1. Imprimir reglas cargadas" << std::endl;
        std::cout << "2. Agregar una regla" << std::endl;
        std::cout << "3. Cerrar el programa" << std::endl;
        std::cout << "Seleccione una opcion: ";
        std::cin >> choice;
        
        switch(choice) {
            case 1:
                printRules(rules);
                break;
            case 2:
                addRule(rules);
                break;
            case 3:
                std::cout << "Cerrando el programa..." << std::endl;
                break;
            default:
                std::cout << "Opcion invalida. Intente de nuevo." << std::endl;
        }
    } while(choice != 3);
    
    return 0;
}