#include "SCHCLoRaWAN_NS_MQTT_Stack.hpp"

SCHCLoRaWAN_NS_MQTT_Stack::SCHCLoRaWAN_NS_MQTT_Stack(AppConfig &appConfig, SCHCCore &schcCore): _appConfig(appConfig), _schcCore(schcCore)
{
    _port       = _appConfig.mqtt.port;
    _host       = _appConfig.mqtt.host;
    _username   = _appConfig.mqtt.username;
    _password   = _appConfig.mqtt.password;   
    _topic_1    = "v3/" + _username + "/devices/+/up";
    
    SPDLOG_DEBUG("Using MQTT parameter - host: {}", _host.c_str());
    SPDLOG_DEBUG("Using MQTT parameter - port: {}", _port);
    SPDLOG_DEBUG("Using MQTT parameter - username: {}", _username);
    SPDLOG_DEBUG("Using MQTT parameter - topic: {}", _topic_1);    



}

SCHCLoRaWAN_NS_MQTT_Stack::~SCHCLoRaWAN_NS_MQTT_Stack()
{
    SPDLOG_DEBUG("Disconnection of the mqtt broker");
    // 1. Disconnect from the server (sends the DISCONNECT packet)
    mosquitto_disconnect(_mosq);

    // 2. Stop the loop thread
    // This will wait for the network thread to finish processing the disconnection.
    mosquitto_loop_stop(_mosq, false);

    // 3. Clear the instance memory
    mosquitto_destroy(_mosq);
    
    // 4. Global cleanup (only if you are no longer going to use Mosquitto in the programme)
    mosquitto_lib_cleanup();

}

std::string SCHCLoRaWAN_NS_MQTT_Stack::send_frame(int ruleid, std::vector<uint8_t>& buff, std::optional<std::string> devId)
{
    std::string topic = "v3/" + std::string(_username) + "/devices/" + devId.value() + "/down/push";

    // Variables con valores dinámicos
    int         f_port_value        = ruleid;
    std::string frm_payload_value   = base64_encode(buff);
    std::string priority_value      = "NORMAL";

    // Crear un objeto JSON con valores provenientes de variables
    json json_object = {
        {"downlinks", {
            {
                {"f_port", f_port_value},
                {"frm_payload", frm_payload_value},
                {"priority", priority_value}
            }
        }}
    };

    // Convertir el objeto JSON a una cadena (string)
    std::string json_string = json_object.dump();

    SPDLOG_DEBUG("Downlink topic: {}", topic);
    SPDLOG_DEBUG("Downlink JSON: {}", json_string);

    if (!_connected) {
        SPDLOG_WARN("Cannot publish: MQTT broker is currently offline.");
        return "ERROR"; 
    }

    int result = mosquitto_publish(_mosq, nullptr, topic.c_str(), 
                                   static_cast<int>(json_string.length()), 
                                   json_string.c_str(), 1, false);

    if (result != MOSQ_ERR_SUCCESS) 
    {
        SPDLOG_ERROR("The message could not be published. Code: {}", mosquitto_strerror(result));
        // Si falla por tubería rota, marcamos como desconectado
        if (result == MOSQ_ERR_NO_CONN || result == MOSQ_ERR_CONN_LOST) {
            _connected = false;
        }
    } 
    else 
    {
        SPDLOG_DEBUG("Message sent successfully");
    }
    
    return "OK";
}

void SCHCLoRaWAN_NS_MQTT_Stack::receive_handler(const std::vector<uint8_t>& frame)
{

}

uint32_t SCHCLoRaWAN_NS_MQTT_Stack::getMtu()
{
    return 0;
}

void SCHCLoRaWAN_NS_MQTT_Stack::init()
{
    mosquitto_lib_init();

    // Create an instance of the MQTT client
    _mosq = mosquitto_new(nullptr, true, this);
    if (!_mosq) {
        SPDLOG_ERROR("Failed to create mosquitto instance.");
        return;
    }   
    
    // Configure user credentials
    SPDLOG_DEBUG("Configure user credentials. Username: {}, Password:{}", _username, _password);
    if (mosquitto_username_pw_set(_mosq, _username.data(), _password.data()) != MOSQ_ERR_SUCCESS) {
        SPDLOG_ERROR("Failed to set username and password.");
        mosquitto_destroy(_mosq);
        mosquitto_lib_cleanup();
        return;
    }

    // Configure callbacks
    mosquitto_connect_callback_set(_mosq, onConnectWrapper);
    mosquitto_message_callback_set(_mosq, onMessageWrapper);
    mosquitto_subscribe_callback_set(_mosq, onSubscribeWrapper);
    mosquitto_disconnect_callback_set(_mosq, onDisconnectWrapper);

    // Connect to the remote broker
    SPDLOG_DEBUG("Connecting to the remote broker. {}:{}", _host, _port);
    if (mosquitto_connect(_mosq, _host.data(), _port, 60) != MOSQ_ERR_SUCCESS) {
        SPDLOG_ERROR("Could not connect to the broker.");
        mosquitto_destroy(_mosq);
        mosquitto_lib_cleanup();
        return;
    }

    SPDLOG_DEBUG("Starting mosquitto library client.");
    mosquitto_loop_start(_mosq); // Create an internal thread and return immediately.
    SPDLOG_DEBUG("Mosquitto client library started.");
}

void SCHCLoRaWAN_NS_MQTT_Stack::onConnectWrapper(mosquitto *mosq, void *obj, int rc)
{
    // We retrieve the class instance
    auto* instance = static_cast<SCHCLoRaWAN_NS_MQTT_Stack*>(obj);
    if (instance) {
        instance->onConnect(mosq, rc); // We call the actual method
    }
}

void SCHCLoRaWAN_NS_MQTT_Stack::onDisconnectWrapper(mosquitto *mosq, void *obj, int rc)
{
    // We retrieve the class instance
    auto* instance = static_cast<SCHCLoRaWAN_NS_MQTT_Stack*>(obj);
    if (instance) {
        instance->onDisconnect(mosq, rc); // We call the actual method
    }
}

void SCHCLoRaWAN_NS_MQTT_Stack::onMessageWrapper(mosquitto *mosq, void *obj, const mosquitto_message *msg)
{
    // We retrieve the class instance
    auto* instance = static_cast<SCHCLoRaWAN_NS_MQTT_Stack*>(obj);
    if (instance) {
        instance->onMessage(mosq, obj, msg); // We call the actual method
    }
}

void SCHCLoRaWAN_NS_MQTT_Stack::onSubscribeWrapper(mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
    // We retrieve the class instance
    auto* instance = static_cast<SCHCLoRaWAN_NS_MQTT_Stack*>(obj);
    if (instance) {
        instance->onSubscribe(mosq, obj, mid, qos_count, granted_qos); // We call the actual method
    }
}

std::vector<uint8_t> SCHCLoRaWAN_NS_MQTT_Stack::base64_decode(std::string encoded)
{
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string decoded;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (base64_chars.find(c) == std::string::npos) break;
        val = (val << 6) + base64_chars.find(c);
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    std::vector<uint8_t> vec(decoded.begin(), decoded.begin() + decoded.size());
   
    return vec;
}

void SCHCLoRaWAN_NS_MQTT_Stack::onConnect(mosquitto *mosq, int rc)
{
    if (this == nullptr) {
        SPDLOG_ERROR("CRASH INMINENTE: 'this' es nulo en onConnect");
        return;
    }

    if (this->_topic_1.empty()) {
        SPDLOG_ERROR("El tópico está vacío, ¿se asignó correctamente antes de conectar?");
    }


    
    if (rc == 0) 
    {
        this->_connected = true;
        SPDLOG_DEBUG("MQTT Broker connected and ready.");
        SPDLOG_DEBUG("Subscribing to Mosquitto Broker with topic {}", this->_topic_1);
        mosquitto_subscribe(this->_mosq, nullptr, this->_topic_1.data(), 0);
    } 
    else 
    {
        SPDLOG_ERROR("Failed to connect, return code: {}", rc);
    }
}

void SCHCLoRaWAN_NS_MQTT_Stack::onDisconnect(mosquitto *mosq, int rc)
{
    if (rc == 0) 
    {
        this->_connected = false;
        SPDLOG_DEBUG("MQTT Broker disconnected.");
    } 
    else 
    {
        SPDLOG_ERROR("Failed to connect, return code: {}", rc);
    }
}

void SCHCLoRaWAN_NS_MQTT_Stack::onMessage(mosquitto *mosq, void *obj, const mosquitto_message *msg)
{
    // Forma segura usando la longitud del mensaje
    std::string raw_payload(static_cast<char*>(msg->payload), msg->payloadlen);
    
    SPDLOG_TRACE("Received message on topic: {} --> {}", msg->topic, raw_payload);

    if (msg->payloadlen > 0)
    {
        auto msgStack       = std::make_unique<StackMessage>();
        json parsed_json = json::parse(raw_payload);
        std::vector<uint8_t> frame;

        if(parsed_json.contains("end_device_ids") && parsed_json["end_device_ids"].contains("device_id"))
        {
            msgStack->deviceId = parsed_json["end_device_ids"]["device_id"];
            SPDLOG_DEBUG("DeviceID: {}", msgStack->deviceId);
        }
        else
        {
            SPDLOG_ERROR("The mqtt JSON message not include the \"end_device_ids\" key");     
        }


        if(parsed_json.contains("uplink_message") && parsed_json["uplink_message"].contains("frm_payload"))
        {
            // save and print encoded buffer
            std::string frm_payload = parsed_json["uplink_message"]["frm_payload"];

            // save and print decoded buffer (text format)
            frame = base64_decode(frm_payload);
            SPDLOG_TRACE("Decoded Payload (hex format): {}", spdlog::to_hex(frame));
            SPDLOG_DEBUG("Decoded Payload Length: {}", frame.size());

            msgStack->len = frame.size();
            msgStack->payload = frame;
        }
        else
        {
            SPDLOG_ERROR("The mqtt JSON message not include the \"uplink_message\" and \"frm_payload\" keys");
            return ; 
        }


        // Obtener y almacenar el rule ID
        uint8_t _rule_id;
        if(parsed_json.contains("uplink_message") && parsed_json["uplink_message"].contains("f_port"))
        {
            // save and print encoded buffer
            _rule_id = parsed_json["uplink_message"]["f_port"];
            SPDLOG_DEBUG("Rule ID: {}", _rule_id);
            msgStack->ruleId = _rule_id;
        }
        else
        {
            SPDLOG_ERROR("The mqtt JSON message not include the \"uplink_message\" and \"f_port\" keys");
            return; 
        }

        _schcCore.enqueueFromStack(std::move(msgStack));
    }
    else
    {
        SPDLOG_ERROR("Message Len is 0");
        return;
    }
}

void SCHCLoRaWAN_NS_MQTT_Stack::onSubscribe(mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	bool have_subscription = false;

	/* In this example we only subscribe to a single topic at once, but a
	 * SUBSCRIBE can contain many topics at once, so this is one way to check
	 * them all. */
	for(i=0; i<qos_count; i++){
        SPDLOG_DEBUG("on_subscribe: {}:granted qos = {}", i, granted_qos[i]);
		if(granted_qos[i] <= 2){
			have_subscription = true;
		}
	}
	if(have_subscription == false){
		/* The broker rejected all of our subscriptions, we know we only sent
		 * the one SUBSCRIBE, so there is no point remaining connected. */
		fprintf(stderr, "Error: All subscriptions rejected.\n");
        SPDLOG_ERROR("Error: All subscriptions rejected.");
		mosquitto_disconnect(mosq);
	}
}

std::string SCHCLoRaWAN_NS_MQTT_Stack::base64_encode(const std::vector<uint8_t>& buffer) 
{
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string encoded;
    // Optimización: Reservamos espacio para evitar múltiples reasignaciones de memoria
    // Base64 ocupa aproximadamente 4/3 del tamaño original
    encoded.reserve(((buffer.size() + 2) / 3) * 4);

    int val = 0;
    int valb = -6;

    // 1. Corregido: Usar buffer.size() en lugar de len
    for (const auto& byte : buffer) {
        val = (val << 8) + byte;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    // 2. Manejo de los bits restantes (Padding)
    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    // 3. Añadir el relleno '=' para cumplir con el estándar de 4 caracteres
    while (encoded.size() % 4) {
        encoded.push_back('=');
    }

    return encoded;
}