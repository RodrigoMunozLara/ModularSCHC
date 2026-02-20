#pragma once

#include <string>
#include <vector>

struct GeneralConfig {
    std::vector<std::string> cores;
};

struct LoggingConfig {
    std::string log_level;
};

struct BackhaulCoreConfig {
    std::string interface_name;
};

struct SCHCCoreConfig {
    std::string schc_l2_protocol;
    std::string schc_type;
    std::string schc_ack_mechanism;
    double error_prob;
    std::string schc_reliability;
    
};

struct MQTTConfig {
    std::string host;
    int port;
    std::string username;
    std::string password;
    std::vector<std::string> devices;
};

struct LoRaWAN_Node_Config {
    std::string data_rate;
    std::string serial_port;
    std::string deveui;
    std::string appeui;
    std::string appkey;
};


struct AppConfig {
    GeneralConfig general;
    LoggingConfig logging;
    LoRaWAN_Node_Config lorawan_node;
    BackhaulCoreConfig backhaul;
    MQTTConfig mqtt;
    SCHCCoreConfig schc;
};
