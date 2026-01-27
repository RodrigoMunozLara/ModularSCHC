#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <limits>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "SCHC_Packet.hpp"
#include "SCHC_RuleID.hpp"
#include "SCHC_RulesParserIni.hpp"
#include "SCHC_PacketParser.hpp"

// Lee una opción de menú de forma segura (evita cin en fail-state)
static int read_menu_choice() {
    while (true) {
        std::cout << "Seleccione una opcion: ";
        std::string s;
        if (!std::getline(std::cin, s)) return 3; // EOF => salir

        // trim simple
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        s = s.substr(i);

        if (s == "1" || s == "2" || s == "3") return (s[0] - '0');

        std::cout << "Opcion invalida. Intente de nuevo.\n";
        spdlog::warn("Input invalido en menu: '{}'", s);
    }
}

static void process_hex_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        spdlog::error("No se pudo abrir archivo de tramas: '{}'", path);
        std::cout << "No se pudo abrir: " << path << "\n";
        return; // vuelve al menú, no mata el programa
    }

    std::string line;
    size_t lineNum = 0;
    size_t ok = 0, fail = 0;

    spdlog::info("Procesando archivo de tramas: '{}'", path);

    while (std::getline(in, line)) {
        ++lineNum;

        // ignora líneas vacías o comentarios
        if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0) continue;

        try {
            auto bytes = hex_to_bytes_one_line(line);
            if (bytes.empty()) continue;

            auto frame = parseIPv6UdpRaw(bytes);

            spdlog::info("OK linea {}: nextHeader={} payload_bytes={}",
                         lineNum, int(frame.ipv6_header.next_header), frame.payload.size());
            ++ok;
        } catch (const std::exception& e) {
            spdlog::error("Error linea {}: {}", lineNum, e.what());
            ++fail;
        }
    }

    spdlog::info("Fin procesamiento '{}': ok={} fail={}", path, ok, fail);
    std::cout << "Procesamiento terminado: ok=" << ok << " fail=" << fail << "\n";

    auto bytes = hex_to_bytes_one_line(line);
    auto frame = parseIPv6UdpRaw(bytes);

    // Mostrar paquete parseado
    dumpIPv6UdpFrame(frame);

}

int main() {
    // ---- Logging a archivo ----
    try {
        auto logger = spdlog::basic_logger_mt("basic_logger", "logs.txt");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        spdlog::info("Programa iniciado.");
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Error configurando logging: " << ex.what() << std::endl;
        return 1;
    }

    // ---- Precarga de reglas ----
    spdlog::info("Cargando reglas desde RulesPreLoad.ini...");
    std::map<uint32_t, LoadedRule> rules;

    try {
        rules = load_rules_ini("RulesPreLoad.ini");
        spdlog::info("Reglas cargadas exitosamente. Numero: {}", rules.size());
    } catch (const std::exception& e) {
        spdlog::error("Error cargando reglas: {}", e.what());
        std::cerr << "Error cargando reglas: " << e.what() << std::endl;
        return 1;
    }

    // ---- Menú ----
    for (;;) {
        std::cout << "\nMenu:\n";
        std::cout << "1. Imprimir reglas cargadas\n";
        std::cout << "2. Cargar/parsear paquete IPv6-UDP desde archivo\n";
        std::cout << "3. Cerrar el programa\n";

        int choice = read_menu_choice();

        if (choice == 1) {
            spdlog::info("Imprimiendo reglas...");
            printRules(rules);
            spdlog::info("Reglas impresas.");
        }
        else if (choice == 2) {
            std::cout << "Ingrese ruta del archivo (Enter = packet/demo.txt): ";
            std::string path;
            std::getline(std::cin, path);
            if (path.empty()) path = "packet/demo.txt";

            process_hex_file(path);

        }
        else if (choice == 3) {
            spdlog::info("Cerrando el programa...");
            std::cout << "Cerrando el programa...\n";
            break;
        }
    }

    spdlog::info("Programa finalizado.");
    return 0;
}
