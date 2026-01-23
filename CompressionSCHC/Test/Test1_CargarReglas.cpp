#include <iostream>
#include <vector>
#include <map>
#include <limits>  // Para std::numeric_limits
#include <spdlog/spdlog.h>  // Agrega para logging
#include <spdlog/sinks/basic_file_sink.h>  // Para log a archivo
#include "SCHC_Packet.hpp"
#include "SCHC_RuleID.hpp"
#include "SCHC_RulesManager.hpp"

// Cambia printRules para trabajar con std::map<std::string, LoadedRule>
void printRules(const std::map<std::string, LoadedRule>& rules) {
    std::cout << "Número de reglas cargadas: " << rules.size() << std::endl;
    for (const auto& pair : rules) {
        const std::string& name = pair.first;
        const LoadedRule& rule = pair.second;
        std::cout << "Regla: " << name << " (ID: " << rule.ruleid << ")" << std::endl;
        if (rule.devid) {
            std::cout << "  DevID: " << *rule.devid << std::endl;
        } else {
            std::cout << "  DevID: NONE" << std::endl;
        }
        std::cout << "  Campos (" << rule.fieldCount << "):" << std::endl;
        for (size_t i = 0; i < rule.fieldCount; ++i) {
            const FieldDescription& f = rule.fields[i];
            std::cout << "    " << f.FID << " - FL: " << (int)f.FL << ", FP: " << f.FP << ", DI: " << (int)f.DI << ", MO: " << (int)f.MO << ", CDA: " << (int)f.CDA << std::endl;
        }
        std::cout << std::endl;
    }
}

// Remueve addRule o ajusta si es necesario (por simplicidad, lo dejo, pero no funcionará directamente con el mapa)

int main() {
    // Configura logging a archivo
    try {
        auto logger = spdlog::basic_logger_mt("basic_logger", "logs.txt");
        spdlog::set_default_logger(logger);
        spdlog::info("Programa iniciado.");
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Error configurando logging: " << ex.what() << std::endl;
        return 1;
    }


    //Codigo para precargar reglas desde archivo INI
    spdlog::info("Cargando reglas desde RulesPreLoad.ini...");
    std::string iniPath = "RulesPreLoad.ini";
    std::unordered_map<uint32_t, LoadedRule> rules;
    try {
        rules = load_rules_ini(iniPath.c_str());
        spdlog::info("Reglas cargadas exitosamente. Número: {}", rules.size());
    } catch (const std::exception& e) {
        spdlog::error("Error cargando reglas: {}", e.what());
        std::cerr << "Error cargando reglas: " << e.what() << std::endl;
        return 1;
    }
    
    int choice;
    do {
        std::cout << "\nMenu:" << std::endl;
        std::cout << "1. Imprimir reglas cargadas" << std::endl;
        std::cout << "2. Cerrar el programa" << std::endl;
        std::cout << "Seleccione una opcion: ";
        std::cin >> choice;
        
        switch(choice) {
            case 1:
                spdlog::info("Imprimiendo reglas...");
                printRules(rules);
                spdlog::info("Reglas impresas.");
                break;
            case 2:
                spdlog::info("Cerrando el programa...");
                std::cout << "Cerrando el programa..." << std::endl;
                break;
            default:
                spdlog::warn("Opción inválida seleccionada: {}", choice);
                std::cout << "Opcion invalida. Intente de nuevo." << std::endl;
        }
    } while(choice != 2);
    
    spdlog::info("Programa finalizado.");
    // Pausa para mantener abierta la consola
    std::cout << "\nPresiona Enter para salir..." << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();

    return 0;
}