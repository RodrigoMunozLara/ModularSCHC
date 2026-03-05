#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <unordered_map>
#include <limits>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include "SCHC_Packet.hpp"
#include "SCHC_Rule.hpp"
#include "SCHC_RulesManager.hpp"
#include "PacketParser.hpp"
#include "SCHC_Compressor.hpp"

// Safely reads a menu option
static int read_menu_choice() {
    while (true) {
        std::cout << "Select an option: ";
        std::string s;
        if (!std::getline(std::cin, s)) return 4; // EOF => exit

        // Simple trim
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        s = s.substr(i);

        if (s == "1" || s == "2" || s == "3" || s == "4") return (s[0] - '0');
        spdlog::warn("Invalid menu input: '{}'", s);
        std::cout << "Invalid option. Please try again.\n";
    }
}

int main() {
    // ---- File Logging ----
    FSM_Ctx ctx;
    CompressorFSM fsm;
    
    ctx.default_ID = 0;
    ctx.arrived = false;
    ctx.OnFSM = false;
    std::vector<uint8_t> raw_pkt;

    try {
        auto logger = spdlog::basic_logger_mt("basic_logger", "logs.txt");
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::trace);
        spdlog::set_level(spdlog::level::trace);
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        
        spdlog::info("\n");
        spdlog::info("------------------------");
        spdlog::info("Program started.");
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log configuration error: " << ex.what() << std::endl;
        return 1;
    }

    // ---- Rules Preloading ----
    const std::string rule_path = "config/rules.json";
    const std::string tmp_path   = "config/rules.tmp.json";
    RuleContext rules;
    
    try {
        spdlog::info("Validating rules.json with YANG");
        validate_json_schc("./yang", rule_path);
        spdlog::info("Loading rules from rules.json");
        rules = load_rules_from_json(rule_path);
        spdlog::info("Rules loaded successfully. Count: {}", rules.size());
    } catch (const std::exception& ex) {
        spdlog::error("Error loading rules: {}", ex.what());
        std::cerr << "Error loading rules: " << ex.what() << std::endl;
        return 1;
    }

    ctx.rulesCtx = &rules;
    ctx.OnFSM = true;
    
    // ---- Menu ----
    for (;;) {
        std::cout << "\nMenu:\n";
        std::cout << "1. Print loaded rules\n";
        std::cout << "2. Create new rule\n";
        std::cout << "3. Start Compression\n";
        std::cout << "4. Close program\n";

        int choice = read_menu_choice();

        if (choice == 1) {
            spdlog::info("Printing rules...");
            printRuleContext(rules);
            spdlog::info("Rules printed.");
            bool sizes = stoul(input_line("\n Do you want to print sizes? Yes:1 No:0 \n"));
            if (sizes) {
                print_sizes(rules);
            }
        }
        else if(choice == 2) {
            std::cout << "You will be prompted for fields to create a new rule,\n";
            std::cout << "as well as the number of Field Descriptors (FID) the rule contains.\n";

            SCHC_Rule newRule;
            create_rule(newRule);

            try {
                spdlog::debug("Writing rule to temporary file");
                write_rule_to_json(rule_path, tmp_path, newRule);
                        
                spdlog::info("Validating rules.tmp.json with YANG");
                validate_json_schc("./yang", tmp_path);
                insert_rule_into_context(rules, newRule);

                std::error_code ec;
                std::filesystem::rename(tmp_path, rule_path, ec);

                if(ec) {
                    std::filesystem::remove(rule_path, ec);
                    ec.clear();
                    std::filesystem::rename(tmp_path, rule_path, ec);
                    if(ec) {
                        spdlog::error("Could not replace rules.json: {}", ec.message());
                        throw std::runtime_error("Could not replace rules.json: " + ec.message());
                    }
                }
                spdlog::info("New rule validated and saved.");

            } catch(const std::exception& e) {
                spdlog::debug("Could not save/validate rule ID={} in JSON: {}", newRule.getRuleID(), e.what());
                std::cout << "Could not save/validate in JSON: " << e.what() << "\n";

                std::error_code ec;
                std::filesystem::remove(tmp_path, ec);
            }

            spdlog::debug("Menu option 2 finished.");

       } else if(choice == 3) {
            while(!ctx.arrived) {
                std::string pkt_num = input_line("Enter the packet number to compress (1-3): ");
                std::string pkt_path = "packets/demo" + pkt_num + ".txt";
                
                if(pkt_num.empty()) {
                    spdlog::warn("No packet path provided. Try again.");
                    std::cout << "Empty path. Please try again.\n";
                    continue;
                }
                try {
                    spdlog::info("Loading packet from '{}'", pkt_path);
                    raw_pkt = hex_to_bytes(read_file_to_string(pkt_path));
                    ctx.raw_pkt = &raw_pkt;
                    ctx.direction = direction_indicator_t::UP; 
                    ctx.arrived = true;

                } catch (const std::exception& e) {
                    spdlog::error("Error loading packet: {}", e.what());
                    std::cout << "Error loading packet: " << e.what() << "\n";
                }
            }

            ctx.OnFSM = true;
            while (ctx.OnFSM) {
                STATE_RESULT r = fsm.stepFSM(ctx);
                if (r == STATE_RESULT::STOP_ || r == STATE_RESULT::ERROR_) {
                    ctx.OnFSM = false;
                    break;
                }
                if (r == STATE_RESULT::STAY_ && !ctx.arrived) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                if (!ctx.arrived) {
                    ctx.OnFSM = false;
                    break;
                }
            }
            ctx.OnFSM = false;
        }
        else if (choice == 4) {
            spdlog::info("Closing program...");
            std::cout << "Closing program...\n";
            break;
        }
    }

    spdlog::info("Program finished.");
    spdlog::shutdown();
    return 0;
}