#include "SCHCSession.hpp"

class SCHCCore; 

SCHCSession::SCHCSession(uint8_t id, SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore): _id(id), _dir(dir), _appConfig(appConfig), _schcCore(schcCore)
{

}

SCHCSession::~SCHCSession()
{
    SPDLOG_INFO("Executing SCHCSession destructor() in session {}", _id);
    SPDLOG_INFO("\033[31mSession completed\033[0m");
    if((_dir == SCHCFragDir::UPLINK_DIR) && (_appConfig.schc.schc_type.compare("schc_node") == 0))
    {
        SPDLOG_INFO("\033[31m************************* Message transmited to LPWAN ***********************************\033[0m");
        SPDLOG_DEBUG("");
        SPDLOG_DEBUG("");
    }
    else if((_dir == SCHCFragDir::DOWNLINK_DIR) && (_appConfig.schc.schc_type.compare("schc_node") == 0))
    {
        SPDLOG_INFO("\033[31m************************* Message transmited to Backhaul*************************************\033[0m");
        SPDLOG_DEBUG("");
        SPDLOG_DEBUG("");        
    }
    else if((_dir == SCHCFragDir::UPLINK_DIR) && (_appConfig.schc.schc_type.compare("schc_gateway") == 0))
    {
        SPDLOG_INFO("\033[31m************************* Message transmited to Backhaul*************************************\033[0m");
        SPDLOG_DEBUG("");
        SPDLOG_DEBUG("");        
    }
    else if((_dir == SCHCFragDir::DOWNLINK_DIR) && (_appConfig.schc.schc_type.compare("schc_gateway") == 0))
    {
        SPDLOG_INFO("\033[31m************************* Message transmited to LPWAN*************************************\033[0m");
        SPDLOG_DEBUG("");
        SPDLOG_DEBUG("");        
    }


}

void SCHCSession::init()
{
    SPDLOG_DEBUG("SCHCSession created with id '{}'", _id);

    if((_dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_node") == 0) 
        && (_appConfig.schc.schc_ack_mechanism.compare("arq_fec") != 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan_at") == 0))
    {
        _stateMachine = std::make_unique<SCHCAckOnErrorSender>(_dir, _appConfig, _schcCore, *this);
    }
    else if((_dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_gateway") == 0) 
        && (_appConfig.schc.schc_ack_mechanism.compare("arq_fec") != 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan_ns_mqtt") == 0))
    {
        _stateMachine = std::make_unique<SCHCAckOnErrorReceiver>(_dir, _appConfig, _schcCore, *this);
    }
    else if((_dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_node") == 0) 
        && (_appConfig.schc.schc_ack_mechanism.compare("arq_fec") == 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan_at") == 0))
    {
        _stateMachine = std::make_unique<SCHCArqFecSender>(_dir, _appConfig, _schcCore, *this);
    }
    else if((_dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_gateway") == 0) 
        && (_appConfig.schc.schc_ack_mechanism.compare("arq_fec") == 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan_ns_mqtt") == 0))
    {
        _stateMachine = std::make_unique<SCHCArqFecReceiver>(_dir, _appConfig, _schcCore, *this);
    }
    else if((_dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_node") == 0) 
        && (_appConfig.schc.schc_ack_mechanism.compare("arq_fec") == 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("myriota_at") == 0))
    {
        _stateMachine = std::make_unique<SCHCArqFecSender>(_dir, _appConfig, _schcCore, *this);
    }
    else if((_dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_gateway") == 0) 
        && (_appConfig.schc.schc_ack_mechanism.compare("arq_fec") == 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("myriota_ns_http") == 0))
    {
        _stateMachine = std::make_unique<SCHCArqFecReceiver>(_dir, _appConfig, _schcCore, *this);
    }
    else if((_dir == SCHCFragDir::DOWNLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_gateway") == 0) 
        && (_appConfig.schc.schc_ack_mechanism.compare("arq_fec") == 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan_ns_mqtt") == 0))
    {
        _stateMachine = std::make_unique<SCHCArqFecSender>(_dir, _appConfig, _schcCore, *this);
    }
    else if((_dir == SCHCFragDir::DOWNLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_node") == 0) 
        && (_appConfig.schc.schc_ack_mechanism.compare("arq_fec") != 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan_at") == 0))
    {
        _stateMachine = std::make_unique<SCHCAckOnErrorReceiver>(_dir, _appConfig, _schcCore, *this);
    }
    else
    {
        SPDLOG_ERROR("There is no setting in the configuration file specifying which state machine to start");
    }

    running.store(true);

    if(_appConfig.lorawan_node.node_class.compare("C") == 0)
    {
        read_sat_pass();
    }
    
    SPDLOG_DEBUG("Starting threads...");
    processThread = std::thread(&SCHCSession::processEventLoop, this);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

void SCHCSession::enqueueEvent(std::unique_ptr<EventMessage> evMsg)
{
    {
        std::lock_guard<std::mutex> lock(eventMtx);
        eventQueue.push(std::move(evMsg));
    }

    eventCv.notify_one();
    SPDLOG_DEBUG("Message enqueued in eventQueue of the session {}", _id);
}

void SCHCSession::processEventLoop()
{
    auto tid = spdlog::details::os::thread_id();
    SPDLOG_DEBUG("Starting SCHCSession::processEventLoop() [GREENthread] with id: {}", tid);
    while (running.load())
    {
        std::unique_ptr<EventMessage> eventMsg;
        int eventQueueSize = 0;

        {
            std::unique_lock<std::mutex> lock(eventMtx);
            eventCv.wait(lock, [&]() {
                return !eventQueue.empty() || !running.load();
            });

            if (!running.load())
            {
                /* Exits the while loop when the StackCore 
                has transitioned to the stop state*/
                SPDLOG_DEBUG("Breaking processEventLoop() loop");
                break;
            }

            eventMsg = std::move(eventQueue.front());
            eventQueue.pop();
            eventQueueSize = eventQueue.size();
        }        


        if(eventMsg->evType == EventType::SCHCCoreMsgReceived)
        {
            SPDLOG_DEBUG("");
            SPDLOG_DEBUG("****** Starting message processing in the session *********");
            SPDLOG_DEBUG("Removing a SCHCCore message from the eventQueue. {} messages remaining.", eventQueueSize);
            SPDLOG_DEBUG("Calling the execute(msg) of the stateMachine");
            

            _stateMachine->execute(eventMsg->payload);

        }
        else if (eventMsg->evType == EventType::StackMsgReceived)
        {
            SPDLOG_DEBUG("");
            SPDLOG_DEBUG("****** Starting message processing in the session *********");
            SPDLOG_DEBUG("Removing a Stack message from the eventQueue. {} messages remaining.", eventQueueSize);
            SPDLOG_DEBUG("Calling the execute(msg) of the stateMachine");
            _stateMachine->execute(eventMsg->payload);
        }
        else if (eventMsg->evType == EventType::TimerExpired)
        {
            SPDLOG_DEBUG("");
            SPDLOG_DEBUG("****** Starting message processing in the session *********");
            SPDLOG_DEBUG("Removing a TimerExpired message from the eventQueue. {} messages remaining.", eventQueueSize);
            SPDLOG_DEBUG("Calling the timerExpired() of the stateMachine");        
            _stateMachine->timerExpired();
        }
        else if (eventMsg->evType == EventType::ExecuteAgain)
        {
            SPDLOG_DEBUG("");
            SPDLOG_DEBUG("****** Starting message processing in the session *********");
            SPDLOG_DEBUG("Removing a ExecuteAgain message from the eventQueue. {} messages remaining.", eventQueueSize);
            SPDLOG_DEBUG("Calling the execute() of the stateMachine");
            _stateMachine->execute();
        }

        SPDLOG_DEBUG("****** Concluding message processing in the session *********");

    }
    
    SPDLOG_DEBUG("SCHCSession::processEventLoop() function finished");

}

void SCHCSession::release()
{
    SPDLOG_DEBUG("Releasing session {}", _id);

    /* Stopping SCHCSession::processEventLoop() */
    SPDLOG_DEBUG("Stopping SCHCSession::processEventLoop()");
    running.store(false);

    /* Release the state in state machine*/
    _stateMachine->release();
    SPDLOG_DEBUG("Releasing the memory of the StateMachine");
    _stateMachine.reset();

    // Wake up TX thread
    eventCv.notify_all();

    if (processThread.joinable()) {
        processThread.join();
    }

    SPDLOG_DEBUG("SCHCSession stopped");
    SPDLOG_DEBUG("Session resources released");
}

int SCHCSession::read_sat_pass()
{
std::string filename = "satellite_passes.csv";
    std::ifstream file(filename);

    if (!file.is_open()) {
        SPDLOG_ERROR("Error opening the file containing satellite timing data: {}", filename );
        SPDLOG_ERROR("Current path: {}", std::filesystem::current_path().string() );
        return 1;
    }


    std::string line;
    
    // Leer y omitir la primera línea (encabezado)
    if (std::getline(file, line)) {
        // Encabezado ignorado
    }

    // Leer el archivo línea por línea
    while (std::getline(file, line)) {
        if (line.empty()) continue; // Omitir líneas vacías

        std::stringstream ss(line);
        std::string field;
        
        std::string time_str;
        std::string duration_str;
        std::string max_elev_str;
        std::string visibility_str;
        std::string revisit_str;

        // Extraer cada columna separada por coma
        std::getline(ss, time_str, ',');
        std::getline(ss, duration_str, ',');
        std::getline(ss, max_elev_str, ',');
        std::getline(ss, visibility_str, ',');
        std::getline(ss, revisit_str, ',');

        // Convertir las cadenas de las últimas dos columnas a enteros y guardarlas
        try {
            int visibility = std::stoi(visibility_str);
            int revisit = std::stoi(revisit_str);

            _visibility_col.push_back(visibility);
            _revisit_col.push_back(revisit);
        } catch (const std::invalid_argument& e) {
            SPDLOG_ERROR("Error converting numeric data in the row: {}", line );
        }
    }

    file.close();

    // Ejemplo para verificar que se leyeron correctamente
    std::cout << "Data read successfully. Total rows:" << _visibility_col.size() << "\n\n";
    // std::cout << "Visibility (s) | Revisit (s)\n";
    // std::cout << "-----------------------------\n";
    // for (size_t i = 0; i < _visibility_col.size(); ++i) {
    //     std::cout << _visibility_col[i] << "            | " << _revisit_col[i] << "\n";
    // }

    return 0;    
}