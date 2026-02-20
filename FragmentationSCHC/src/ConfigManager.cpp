#include "ConfigManager.hpp"


bool loadConfig(const std::string& filePath, AppConfig& config) 
{
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "Could not open configuration file: " << filePath << "\n";
        return false;
    }

    json j;
    inFile >> j;

    // General
    config.general.cores = j["general"]["cores"].get<std::vector<std::string>>();

    // Logging
    config.logging.log_level = j["logging"]["log_level"].get<std::string>();

    // Ethernet
    config.backhaul.interface_name = j["backhaul_core"]["interface"].get<std::string>();

    // SCHC
    config.schc.schc_l2_protocol = j["schc_core"]["schc_l2_protocol"].get<std::string>();
    config.schc.schc_type = j["schc_core"]["schc_type"].get<std::string>();
    config.schc.schc_ack_mechanism = j["schc_core"]["schc_ack_mechanism"].get<std::string>();
    config.schc.error_prob = std::stod(j["schc_core"]["error_prob"].get<std::string>());
    config.schc.schc_reliability = j["schc_core"]["schc_reliability"].get<std::string>();

    // MQTT
    config.mqtt.host = j["mqtt"]["host"].get<std::string>();
    config.mqtt.port = std::stoi(j["mqtt"]["port"].get<std::string>());
    config.mqtt.username = j["mqtt"]["username"].get<std::string>();
    config.mqtt.password = j["mqtt"]["password"].get<std::string>();
    config.mqtt.devices = j["mqtt"]["devices"].get<std::vector<std::string>>();

    // LoRaWAN Node
    config.lorawan_node.serial_port = j["lorawan_node"]["serial_port"].get<std::string>();
    config.lorawan_node.deveui = j["lorawan_node"]["deveui"].get<std::string>();
    config.lorawan_node.appeui = j["lorawan_node"]["appeui"].get<std::string>();
    config.lorawan_node.appkey = j["lorawan_node"]["appkey"].get<std::string>();
    config.lorawan_node.data_rate = j["lorawan_node"]["data_rate"].get<std::string>();
    

    return true;
}

spdlog::level::level_enum parseLogLevel(const std::string& levelStr)
{
    std::string l = levelStr;
    std::transform(l.begin(), l.end(), l.begin(), ::toupper);

    if (l == "TRACE")
    {
        return spdlog::level::trace;
    } 
    else if (l == "DEBUG")
    {
        return spdlog::level::debug;
    }
    else if (l == "INFO")
    {
        return spdlog::level::info;
    }
    else if (l == "WARN")
    {
        return spdlog::level::warn;
    }
    else if (l == "ERROR")
    {
        return spdlog::level::err;
    }
    else if (l == "CRITICAL")
    {
        return spdlog::level::critical;
    }
    else if (l == "OFF")
    {
        return spdlog::level::off;
    }

    return spdlog::level::info; // Por defecto
}

