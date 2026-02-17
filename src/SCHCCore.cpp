#include "SCHCCore.hpp"
#include "SCHCSession.hpp"
#include <iostream>

SCHCCore::SCHCCore(Orchestrator& orch, AppConfig& appConfig): orchestrator(orch), _appConfig(appConfig)
{
    SPDLOG_DEBUG("Executing SCHCCore constructor()");
}

SCHCCore::~SCHCCore()
{
    SPDLOG_DEBUG("Executing SCHCCore destructor()");
}

CoreId SCHCCore::id()
{
    return CoreId::SCHC;
}

void SCHCCore::start()
{
    if (running.load())
    {
        SPDLOG_ERROR("The SCHCCore::start() method cannot be called when the SCHCCore is already running.");
        return;
    }
    else
    {
        SPDLOG_DEBUG("Setting the SCHCCore as active");
        running.store(true);
    }

    /* Creating stack according to layer 2 protocol */
    SPDLOG_DEBUG("Creating stack object according to layer 2 protocol");
    if(_appConfig.schc.schc_l2_protocol.compare("lorawan") == 0)
    {
        _stack = std::make_unique<SCHCLoRaWANStack>(_appConfig, *this);
        _stack->init();

        /* In LoRaWAN there are only two session: uplink and downlink */
        _uplinkSessionCounterMax = 1;
        _uplinkSessionCounter = 0;
        _downlinkSessionCounterMax = 1;
        _downlinkSessionCounter = 0;
    }
    else if (_appConfig.schc.schc_l2_protocol.compare("sigfox") == 0)
    {
        /* ToDo: Create pool sessions*/

        SPDLOG_DEBUG("Pool of sessions created for SIGFOX");
    }
    else if (_appConfig.schc.schc_l2_protocol.compare("nbiot") == 0)
    {
        /* ToDo: Create pool sessions*/

        SPDLOG_DEBUG("Pool of sessions created for NB-IoT");
    }
    else if (_appConfig.schc.schc_l2_protocol.compare("myriota") == 0)
    {
        /* ToDo: Create pool sessions*/

        SPDLOG_DEBUG("Pool of sessions created for MYRIOTA");
    }
    else if (_appConfig.schc.schc_l2_protocol.compare("lorawan_ns_mqtt") == 0)
    {
        _stack = std::make_unique<SCHCLoRaWAN_NS_MQTT_Stack>(_appConfig, *this);
        _stack->init();

        /* In LoRaWAN there are only two session: uplink and downlink */
        _uplinkSessionCounterMax = 1;
        _uplinkSessionCounter = 0;
        _downlinkSessionCounterMax = 1;
        _downlinkSessionCounter = 0;
    }





    SPDLOG_DEBUG("Starting SCHCCore threads...");
    rxThread = std::thread(&SCHCCore::runRx, this);
    txThread = std::thread(&SCHCCore::runTx, this);
    cleanerThread = std::thread(&SCHCCore::runCleaner, this);
    SPDLOG_DEBUG("SCHCCore threads started...");


    SPDLOG_DEBUG("SCHCCore STARTED");
}

void SCHCCore::stop()
{
    SPDLOG_DEBUG("Stopping all sessions");
    std::lock_guard<std::mutex> lock(sessionsMtx);
    for (auto& [sessionId, sessionPtr] : uplinkSessions)
    {
        if (sessionPtr)
        {
            sessionPtr->release();
        }
    }

    for (auto& [sessionId, sessionPtr] : downlinkSessions)
    {
        if (sessionPtr)
        {
            sessionPtr->release();
        }
    }

    running.store(false);

    // Wake up TX thread
    txCv.notify_all();
    protoCv.notify_all();
    cleanerCv.notify_all();

    if (rxThread.joinable()) {
        rxThread.join();
    }

    if (txThread.joinable()) {
        txThread.join();
    }

    if (cleanerThread.joinable()) {
        cleanerThread.join();
    }

    SPDLOG_DEBUG("SCHCCore successfully stopped");
}

void SCHCCore::runTx()
{
    /*
    * - Remove message from txQueue
    * - Obtain an UPLINK or DOWNLINK unassigned session
    * - Start compress process
    * - Check whether fragmentation is necessary
    * - Start fragmentation process
    * - Release session
    * */

    SPDLOG_DEBUG("Starting SCHCCore::runTx() thread...");
    int i = 1;
    while (running.load())
    {
        std::unique_ptr<RoutedMessage> msg;

        {
            std::unique_lock<std::mutex> lock(txMtx);
            txCv.wait(lock, [&]() {
                return !txQueue.empty() || !running.load();
            });

            if (!running.load() && txQueue.empty())
            {
                /* Exits the while loop when the StackCore 
                has transitioned to the stop state and there 
                are no messages in the queue.*/
                break;
            }

            msg = std::move(txQueue.front());
            txQueue.pop();
        }

        if (!msg) {
            continue;
        }

        if(_appConfig.schc.schc_type.compare("schc_node") == 0)
        {
            SPDLOG_DEBUG("Message received from the Orchestrator, obtaining an unassigned UPLINK session");
            /* Obtaining a unsigned session*/
            if (_uplinkSessionCounter <= _uplinkSessionCounterMax)
            {
                /* Create a new uplink session to process the message from Orchestator */
                /* The currentId is the Dtag in a SCHC session. 
                If the SCHC session does not have Dtag (like in LoRaWAN), the currentID is the RuleID*/

                uint8_t currentId = 20 + _uplinkSessionCounter;

                auto session = std::make_unique<SCHCSession>(currentId, SCHCFragDir::UPLINK_DIR, _appConfig, *this);

                std::lock_guard<std::mutex> lock(sessionsMtx);
                auto [it, inserted] = uplinkSessions.try_emplace(currentId, std::move(session));

                if (inserted) 
                {
                    it->second->init(); 
                    
                    SPDLOG_DEBUG("UPLINK Session with id '{}' started", currentId);

                    auto evMsg = std::make_unique<EventMessage>();
                    evMsg->payload = msg->payload;
                    evMsg->evType = EventType::SCHCCoreMsgReceived;

                    it->second->enqueueEvent(std::move(evMsg));
                    _uplinkSessionCounter++;
                } 
                else 
                {
                    SPDLOG_WARN("Session ID {} already exists, ignoring message", currentId);
                }
            }
            else
            {
                SPDLOG_WARN("No free SCHC uplinkSessions available");
            }
        }

        SPDLOG_DEBUG("SCHCCore::runTx() loop counter: {}" , i);
        i++;
    }

    SPDLOG_DEBUG("SCHCCore::runTx() function finished");
}

void SCHCCore::runRx()
{
    /*
    * - Remove message from protoQueue. protoQueue is used to receive message from Protocol Stack
    * - Obtain an unassigned UPLINK or DOWNLINK session or obtain a session related to the received message.
    * - If you decide get an unassigned UPLINK or DOWNLINK session:
    *       - Check whether reassembly is necessary
    *       - Start reassembly process
    *       - Start uncompress process
    *       - Send reassembly packet to Orchestator
    *       - Release session
    * - If you decide get a session related to the received message:
    *       - obtain the session using the Id session.
    *       - 
    * */

    SPDLOG_DEBUG("Starting SCHCCore::runRx() thread...");
    while (running.load())
    {
        std::unique_ptr<StackMessage> msg;

        {
            std::unique_lock<std::mutex> lock(protoMtx);
            protoCv.wait(lock, [&]() {
                return !protoQueue.empty() || !running.load();
            });

            if (!running.load() && protoQueue.empty())
            {
                /* Exits the while loop when the StackCore 
                has transitioned to the stop state and there 
                are no messages in the protoQueue.*/
                break;
            }

            msg = std::move(protoQueue.front());
            protoQueue.pop();
        }

        if (!msg) {
            continue;
        }


        if((_appConfig.schc.schc_type.compare("schc_gateway") == 0) 
            && (_appConfig.schc.schc_l2_protocol.compare("lorawan_ns_mqtt") == 0))
        {
            uint8_t currentId;

            /* Identifying whether the message is an uplink or downlink message */
            if(msg->ruleId == 20)
            {
                SPDLOG_DEBUG("Receiving an UPLINK message");
                currentId = msg->ruleId;
                {
                    std::lock_guard<std::mutex> lock(sessionsMtx);
                    auto it = uplinkSessions.find(currentId);

                    if (it == uplinkSessions.end()) 
                    {
                        SPDLOG_DEBUG("Creating a new session with ID {}, and storing it on the map", currentId);
                        auto [it2, inserted] = uplinkSessions.try_emplace(currentId, std::make_unique<SCHCSession>(currentId, SCHCFragDir::UPLINK_DIR, _appConfig, *this));

                        it2->second->init();
                        it2->second->setDevId(msg->deviceId);
                        
                        SPDLOG_DEBUG("UPLINK Session with id '{}' started", currentId);

                        auto evMsg = std::make_unique<EventMessage>();
                        evMsg->payload = msg->payload;
                        evMsg->evType = EventType::StackMsgReceived;

                        it2->second->enqueueEvent(std::move(evMsg));
                        _uplinkSessionCounter++;
                    }
                    else
                    {
                        SPDLOG_DEBUG("There is already a session associated with the message with ID {}", currentId);
                        auto evMsg = std::make_unique<EventMessage>();
                        evMsg->payload = msg->payload;
                        evMsg->evType = EventType::StackMsgReceived;

                        it->second->enqueueEvent(std::move(evMsg));
                    }
                }/* Mutex */
            } /* Uplink Message */
        } /* SCHC Gateway + LoRaWAN_NT */     
        else if ((_appConfig.schc.schc_type.compare("schc_node") == 0) 
            && (_appConfig.schc.schc_l2_protocol.compare("lorawan") == 0))
        {
            uint8_t currentId;
            SPDLOG_DEBUG("msg->ruleId: {}", msg->ruleId);
            /* Identifying whether the message is an uplink or downlink message */
            if(msg->ruleId == 20)
            {
                SPDLOG_DEBUG("Receiving an UPLINK response message");
                currentId = msg->ruleId;
                {
                    std::lock_guard<std::mutex> lock(sessionsMtx);
                    auto it = uplinkSessions.find(currentId);

                    if (it == uplinkSessions.end()) 
                    {
                        SPDLOG_WARN("A UPLINK response cannot generate a new session. The message could be a delayed message.");
                    }
                    else
                    {
                        SPDLOG_DEBUG("There is already a session associated with the message with ID {}", currentId);
                        auto evMsg = std::make_unique<EventMessage>();
                        evMsg->payload = msg->payload;
                        evMsg->evType = EventType::StackMsgReceived;

                        it->second->enqueueEvent(std::move(evMsg));
                    }
                }/* Mutex */
            }



        }
    }/*Loop*/

    SPDLOG_DEBUG("SCHCCore::runRx() thread finished");
}

void SCHCCore::runCleaner()
{
    SPDLOG_DEBUG("Starting SCHCCore::runCleaner() thread...");
    //int i = 1;
    while (running.load())
    {
        std::unique_lock<std::mutex> lock(cleanerMtx);
        cleanerCv.wait_for(lock, std::chrono::milliseconds(2000), [this] {
            return !running.load(); // Si running es false, despierta ya
        });

        if (!running.load()) break; // Salida inmediata si ya no estamos corriendo
        
        {
            std::lock_guard<std::mutex> lock(sessionsMtx);

            for (auto it = uplinkSessions.begin(); it != uplinkSessions.end(); )
            {
                if (it->second->getDead()) 
                {
                    SPDLOG_DEBUG("Cleaner: Releasing uplinkSession ID {}", it->first);
                    
                    it->second->release();
                    it = uplinkSessions.erase(it);
                    _uplinkSessionCounter--;
                }
                else 
                {
                    ++it;
                }
            }

            for (auto it = downlinkSessions.begin(); it != downlinkSessions.end(); )
            {
                if (it->second->getDead()) 
                {
                    spdlog::info("Cleaner: Releasing downlinkSession ID {}", it->first);
                    
                    it->second->release();
                    it = downlinkSessions.erase(it); 
                    _downlinkSessionCounter--;
                }
                else 
                {
                    ++it;
                }
            }

        }

        //SPDLOG_DEBUG("Cleaner cycle counter {}", i);
        //i++;

    }

    SPDLOG_DEBUG("SCHCCore::runCleaner() function finished");
}

void SCHCCore::enqueueFromOrchestator(std::unique_ptr<RoutedMessage> msg)
{
    {
        std::lock_guard<std::mutex> lock(txMtx);
        txQueue.push(std::move(msg));
    }

    txCv.notify_one();
    SPDLOG_DEBUG("Message enqueued in SCHCCore txQueue");
}

void SCHCCore::enqueueFromStack(std::unique_ptr<StackMessage> msg)
{
    SPDLOG_DEBUG("Enqueue Message in protoQueue");
    {
        std::lock_guard<std::mutex> lock(protoMtx);
        protoQueue.push(std::move(msg));
    }

    protoCv.notify_one();
    SPDLOG_DEBUG("Message enqueued in protoQueue");

}

