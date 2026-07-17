#pragma once

#include "SCHCCore.hpp"
#include "interfaces/ISCHCStack.hpp"
#include "ConfigStructs.hpp"
#include <cstdint>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <mosquitto.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <atomic>

#include <queue>       // <--- ESTE FALTA (Define std::queue)
#include <mutex>       // Para proteger la cola entre hilos (std::mutex, std::lock_guard)
#include <thread>      // Para el hilo del scheduler (std::thread)
#include <chrono>      // Para manejar los tiempos de simulación (std::chrono)

// forward declaration
class SCHCCore;


class SCHCLoRaWAN_NS_MQTT_Stack: public ISCHCStack
{
    public:
        SCHCLoRaWAN_NS_MQTT_Stack(AppConfig& appConfig, SCHCCore& schcCore);
        ~SCHCLoRaWAN_NS_MQTT_Stack() override;
        std::string send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId = std::nullopt) override;
        void receive_handler(const std::vector<uint8_t>& frame) override;
        uint32_t getMtu() override;
        void init() override;
    private:
        static void             onConnectWrapper(struct mosquitto *mosq, void *obj, int rc);
        static void             onDisconnectWrapper(struct mosquitto *mosq, void *obj, int rc);
        static void             onMessageWrapper(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg);
        static void             onSubscribeWrapper(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
        std::vector<uint8_t>    base64_decode(std::string encoded);
        std::string             base64_encode(const std::vector<uint8_t>& buffer);

        void                    onConnect(struct mosquitto *mosq, int rc);
        void                    onDisconnect(struct mosquitto *mosq, int rc);
        void                    onMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg);
        void                    onSubscribe(mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);

        AppConfig           _appConfig;
        SCHCCore&           _schcCore;

        int                 _dr;
        std::string         _dev_id;

        struct mosquitto*   _mosq;
        int                 _port;
        std::string         _host;
        std::string         _username;
        std::string         _password;   
        std::string         _topic_1;

        std::atomic<bool>   _connected{false};


    /* ************ Para simular satelite LEO ************* */ 
    private:
        struct DelayedUplink {
            std::unique_ptr<StackMessage> msg;
            std::chrono::steady_clock::time_point target_time; // Cuándo debe entregarse
        };

        std::queue<DelayedUplink> _delay_queue;
        std::mutex _delay_mutex;
        std::thread _scheduler_thread;
        bool _running = true;

        void scheduler_loop(); // Método que corre en segundo plano

};