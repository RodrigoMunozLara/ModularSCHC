#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <unordered_map>
#include <limits>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include "SCHC_Packet.hpp"
#include "SCHC_Rule.hpp"
#include "SCHC_RulesManager.hpp"
#include "PacketParser.hpp"
#include "SCHC_Compressor.hpp"
#include "SCHC_Yang.hpp"

// Lee una opción de menú de forma segura (evita cin en fail-state)
static int read_menu_choice() {
    while (true) {
        std::cout << "Seleccione una opcion: ";
        std::string s;
        if (!std::getline(std::cin, s)) return 4; // EOF => salir

        // trim simple
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        s = s.substr(i);

        if (s == "1" || s == "2" || s == "3" || s == "4") return (s[0] - '0');
        spdlog::warn("Input invalido en menu: '{}'", s);
        std::cout << "Opcion invalida. Intente de nuevo.\n";
        
    }
}

int main() {
    // ---- Logging a archivo ----
    FSM_Ctx ctx;
    CompressorFSM fsm;
    
    ctx.default_ID = 0;
    ctx.arrived = false;
    ctx.OnFSM = false;

    try {
        auto logger = spdlog::basic_logger_mt("basic_logger", "logs.txt");
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::trace);
        spdlog::set_level(spdlog::level::trace);
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        
        spdlog::info("\n");
        
        spdlog::info("------------------------");
        spdlog::info("Programa iniciado.");
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Error configurando logging: " << ex.what() << std::endl;
        return 1;
    }

    // ---- Precarga de reglas ----
    const std::string rule_path = "config/rules.json";
    const std::string tmp_path   = "config/rules.tmp.json";
    RuleContext rules;
    
    try {
        spdlog::info("Validando Rules.json con yang");
        validate_json_schc("./yang", rule_path);
        spdlog::info("Cargando reglas desde Rules.json");
        rules = load_rules_from_json("config/rules.json");
        spdlog::info("Reglas cargadas exitosamente. Numero: {}", rules.size());
    } catch (const spdlog::spdlog_ex& ex) {
        spdlog::error("Error cargando reglas: {}", ex.what());
        std::cerr << "Error cargando reglas: " << ex.what() << std::endl;
        return 1;
    }

    ctx.rulesCtx = &rules;
    ctx.OnFSM = true;
    
    // ---- Menú ----
    for (;;) {
        std::cout << "\nMenu:\n";
        std::cout << "1. Imprimir reglas cargadas\n";
        std::cout << "2. Crear nueva regla\n";
        std::cout << "3. Empezar Compresión\n";
        std::cout << "4. Cerrar el programa\n";

        int choice = read_menu_choice();

        if (choice == 1) {
            spdlog::info("Imprimiendo reglas...");
            printRuleContext(rules);
            spdlog::info("Reglas impresas.");
            bool sizes = stoul(input_line("\n Desea imprimir tamaños? Si:1  No: 0 \n"));
            if (sizes)
            {
             print_sizes(rules);
            }

        }
        else if(choice == 2) {
            std::cout << "Se pedirán los campos en un formato para crear una nueva regla\n";
            std::cout << "como también cuantos descriptores de campo (FID) contenga la regla\n";

            SCHC_Rule newRule;
            create_rule(newRule);

            try{
                spdlog::debug("Escribiendo regla en archivo temporal");
                write_rule_to_json(rule_path, tmp_path, newRule);
                        
                spdlog::info("Validando rules.tmp.json con yang");
                validate_json_schc("./yang", tmp_path);
                insert_rule_into_context(rules, newRule);

                std::error_code ec;
                std::filesystem::rename(tmp_path, rule_path, ec);

                if(ec){
                    std::filesystem::remove(rule_path,ec);
                    ec.clear();
                    std::filesystem::rename(tmp_path, rule_path, ec);
                    if(ec) {
                        spdlog::error("No se pudo reemplazar rules.json {}", ec.message());
                        throw std::runtime_error("No se pudo reemplazar rules.json"+ec.message());
                        
                    }
                }
                spdlog::info("regla nueva validada y guarda");

                
            }catch(const std::exception& e){
                spdlog::debug("No se pudo guardar/validar regla ID={} en json: {}", newRule.getRuleID(), e.what());
                std::cout << "No se pudo guardar/validar en json: " << e.what() << "\n";

                std::error_code ec;
                std::filesystem::remove(tmp_path, ec);
            }

            spdlog::debug("terminó opción 2 del menú");

        }else if(choice == 3){
            while(!ctx.arrived) {
                std::string pkt_path = "packets/demo" + input_line("Ingrese el numero del paquete a comprimir (1-3): ") + ".txt";
                if(pkt_path.empty()) {
                    spdlog::warn("No se ingresó ruta de paquete. Intente de nuevo.");
                    std::cout << "Ruta vacía. Intente de nuevo.\n";
                    continue;
                }
                try {
                    spdlog::info("Cargando paquete desde '{}'", pkt_path);
                    std::vector<uint8_t> raw_pkt = hex_to_bytes(read_file_to_string(pkt_path));
                    ctx.raw_pkt = &raw_pkt;
                    ctx.direction = direction_indicator_t::DOWN; // Asumimos downlink para este demo
                    ctx.arrived = true;

                } catch (const std::exception& e) {
                    spdlog::error("Error cargando paquete: {}", e.what());
                    std::cout << "Error cargando paquete: " << e.what() << "\n";
                }
                fsm.runFSM(ctx);
            
            }
            
            

        }
        else if (choice == 4) {
            spdlog::info("Cerrando el programa...");
            std::cout << "Cerrando el programa...\n";
            break;
        }
    }

    spdlog::info("Programa finalizado.");
    spdlog::shutdown();
    return 0;
}
