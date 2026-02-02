#include "ConfigManager.hpp"


bool loadConfig(const std::string& filePath, AppConfig& config) 
{
    SPDLOG_TRACE("Entering the function");
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

    // LoRaWAN Node
    config.lorawan_node.deviceId = j["lorawan_node"]["deviceId"].get<std::string>();

    SPDLOG_TRACE("Leaving the function");
    return true;
}

spdlog::level::level_enum parseLogLevel(const std::string& levelStr)
{
    SPDLOG_TRACE("Entering the function");
    std::string l = levelStr;
    std::transform(l.begin(), l.end(), l.begin(), ::toupper);

    if (l == "TRACE")
    {
        SPDLOG_TRACE("Leaving the function");
        return spdlog::level::trace;
    } 
    else if (l == "DEBUG")
    {
        SPDLOG_TRACE("Leaving the function");
        return spdlog::level::debug;
    }
    else if (l == "INFO")
    {
        SPDLOG_TRACE("Leaving the function");
        return spdlog::level::info;
    }
    else if (l == "WARN")
    {
        SPDLOG_TRACE("Leaving the function");
        return spdlog::level::warn;
    }
    else if (l == "ERROR")
    {
        SPDLOG_TRACE("Leaving the function");
        return spdlog::level::err;
    }
    else if (l == "CRITICAL")
    {
        SPDLOG_TRACE("Leaving the function");
        return spdlog::level::critical;
    }
    else if (l == "OFF")
    {
        SPDLOG_TRACE("Leaving the function");
        return spdlog::level::off;
    }

    SPDLOG_TRACE("Leaving the function");
    return spdlog::level::info; // Por defecto
}

