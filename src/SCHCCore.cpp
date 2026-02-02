#include "SCHCCore.hpp"
#include "SCHCSession.hpp"

SCHCCore::SCHCCore(Orchestrator& orch, AppConfig& appConfig): orchestrator(orch), _appConfig(appConfig)
{
    SPDLOG_INFO("Executing SCHCCore constructor()");
}

SCHCCore::~SCHCCore()
{
    SPDLOG_INFO("Executing SCHCCore destructor()");
}

CoreId SCHCCore::id()
{
    SPDLOG_TRACE("Entering the function");
    return CoreId::SCHC;
    SPDLOG_TRACE("Leaving the function");
}

void SCHCCore::start()
{
    SPDLOG_TRACE("Entering the function");
    if (running.load())
    {
        SPDLOG_ERROR("The SCHCCore::start() method cannot be called when the SCHCCore is already running.");
        return;
    }
    else
    {
        SPDLOG_ERROR("Setting the SCHCCore as active");
        running.store(true);
    }

    /* Creating stack and session pools according to layer 2 protocol */
    SPDLOG_INFO("Creating stack object according to layer 2 protocol");
    SPDLOG_INFO("Creating session pool according to layer 2 protocol");
    if(_appConfig.schc.schc_l2_protocol.compare("lorawan") == 0)
    {
        /* Creating protocol stack*/
        _stack = std::make_unique<SCHCLoRaWANStack>(_appConfig, *this);


        /* In LoRaWAN there are only two session: uplink and downlink*/
        uplinkSessions.try_emplace(20, std::make_unique<SCHCSession>(20, SCHCFragDir::UPLINK_DIR, _appConfig, *this));      /* RuleID = 20 (8-bit) for uplink fragmentation, named FPortUp. */
        downlinkSessions.try_emplace(21, std::make_unique<SCHCSession>(21, SCHCFragDir::DOWNLINK_DIR, _appConfig, *this));  /* RuleID = 21 (8-bit) for downlink fragmentation, named FPortDown. */
        SPDLOG_INFO("Pool of sessions created for LoRaWAN");
    }
    else if (_appConfig.schc.schc_l2_protocol.compare("sigfox") == 0)
    {
        /* ToDo: Create pool sessions*/

        SPDLOG_INFO("Pool of sessions created for SIGFOX");
    }
    else if (_appConfig.schc.schc_l2_protocol.compare("nbiot") == 0)
    {
        /* ToDo: Create pool sessions*/

        SPDLOG_INFO("Pool of sessions created for NB-IoT");
    }
    else if (_appConfig.schc.schc_l2_protocol.compare("myriota") == 0)
    {
        /* ToDo: Create pool sessions*/

        SPDLOG_INFO("Pool of sessions created for MYRIOTA");
    }
    else if (_appConfig.schc.schc_l2_protocol.compare("lorawan_ns") == 0)
    {
        /* ToDo: Create pool sessions*/

        SPDLOG_INFO("Pool of sessions created for LORAWAN Network Server");
    }

    



    SPDLOG_INFO("Starting threads...");
    //rxThread = std::thread(&SCHCCore::runRx, this);
    txThread = std::thread(&SCHCCore::runTx, this);
    //SPDLOG_INFO("SCHCCore::runRx thread STARTED");
    SPDLOG_INFO("SCHCCore::runTx thread STARTED");

    SPDLOG_INFO("SCHCCore STARTED");
    SPDLOG_TRACE("Leaving the function");
}

void SCHCCore::stop()
{
    SPDLOG_TRACE("Entering the function");

    running.store(false);

    // Wake up TX thread
    txCv.notify_all();

    if (rxThread.joinable()) {
        rxThread.join();
    }

    if (txThread.joinable()) {
        txThread.join();
    }

    SPDLOG_INFO("SCHCCore stopped");
    SPDLOG_TRACE("Leaving the function");
}

void SCHCCore::enqueueFromOrchestator(std::unique_ptr<RoutedMessage> msg)
{
    {
        std::lock_guard<std::mutex> lock(txMtx);
        txQueue.push(std::move(msg));
    }

    txCv.notify_one();
    SPDLOG_INFO("Message enqueued in txQueue");
}

void SCHCCore::runTx()
{
    SPDLOG_TRACE("Entering the function");
    /*
    * - Remove message from txQueue
    * - Obtain an UPLINK or DOWNLINK unassigned session
    * - Start compress process
    * - Check whether fragmentation is necessary
    * - Start fragmentation process
    * - Release session
    * */

    SPDLOG_INFO("Starting SCHCCore::runTx() thread...");
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
            SPDLOG_INFO("Message received from the Orchestrator, obtaining an unassigned UPLINK session");
            /* Obtaining a unsigned session*/
            if (auto* session = findUnassignedSession(uplinkSessions))
            {
                session->assign();
                SPDLOG_INFO("UPLINK Session with id '{}' is used to process message", session->getId());

                
                char* data = reinterpret_cast<char*>(msg->payload.data());
                size_t len = msg->payload.size();
                
                SPDLOG_INFO("Starting SCHC compression process");
                /* ToDo: Call a compression class or process */
                /* Debiese ser algo como session.startCompression(data, len)*/
                SPDLOG_INFO("Compression process finished");

                /* ToDo: check whether fragmentation is necessary*/

                SPDLOG_INFO("Starting SCHC fragmentation process");
                session->startFragmentation(data, len);
                SPDLOG_INFO("Fragmentation process finished");

                session->release();

            }
            else
            {
                SPDLOG_WARN("No free SCHC uplinkSessions available");
            }
        }
    }

    SPDLOG_INFO("SCHCCore::runTx() thread finished");
    SPDLOG_TRACE("Leaving the function");
}

void SCHCCore::enqueueFromStack(std::unique_ptr<RoutedMessage> msg)
{
    SPDLOG_INFO("Enqueue Message in protoQueue");
    {
        std::lock_guard<std::mutex> lock(protoMtx);
        protoQueue.push(std::move(msg));
    }

    protoCv.notify_one();
    SPDLOG_INFO("Message enqueued in protoQueue");

}

void SCHCCore::runRx()
{
   SPDLOG_TRACE("Entering the function");
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

    SPDLOG_INFO("Starting SCHCCore::runRx() thread...");
    while (running.load())
    {
        std::unique_ptr<RoutedMessage> msg;

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


    }

    SPDLOG_INFO("SCHCCore::runTx() thread finished");
    SPDLOG_TRACE("Leaving the function");
}

SCHCSession* SCHCCore::findUnassignedSession(std::map<uint32_t, std::unique_ptr<SCHCSession>> &sessions)
{
    for (auto it = sessions.begin(); it != sessions.end(); ++it)
    {
        uint32_t sessionId = it->first;
        SCHCSession* session = it->second.get();

        if (!session->isAssigned())
        {
            return session;
        }
    }

    return nullptr;
}
