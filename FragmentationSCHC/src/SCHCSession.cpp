#include "SCHCSession.hpp"

class SCHCCore; 

SCHCSession::SCHCSession(uint8_t id, SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore): _id(id), _dir(dir), _appConfig(appConfig), _schcCore(schcCore)
{

}

SCHCSession::~SCHCSession()
{
    SPDLOG_INFO("Executing SCHCSession destructor() in session {}", _id);
    SPDLOG_INFO("\033[31mSession completed\033[0m");
}

void SCHCSession::init()
{
    SPDLOG_DEBUG("SCHCSession created with id '{}'", _id);

    if((_dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_node") == 0) 
        && (_appConfig.schc.schc_reliability.compare("false") == 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan") == 0))
    {
        _stateMachine = std::make_unique<SCHCAckOnErrorSender>(_dir, _appConfig, _schcCore, *this);
    }
    else if((_dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_gateway") == 0) 
        && (_appConfig.schc.schc_reliability.compare("false") == 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan_ns_mqtt") == 0))
    {
        _stateMachine = std::make_unique<SCHCAckOnErrorReceiver>(_dir, _appConfig, _schcCore, *this);
    }

    running.store(true);

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
        }        


        if(eventMsg->evType == EventType::SCHCCoreMsgReceived)
        {
            SPDLOG_DEBUG("");
            SPDLOG_DEBUG("****** Starting message processing in the session *********");
            SPDLOG_DEBUG("Removing a SCHCCore message from the eventQueue");
            SPDLOG_DEBUG("Calling the execute(msg) of the stateMachine");
            

            _stateMachine->execute(eventMsg->payload);

        }
        else if (eventMsg->evType == EventType::StackMsgReceived)
        {
            SPDLOG_DEBUG("");
            SPDLOG_DEBUG("****** Starting message processing in the session *********");
            SPDLOG_DEBUG("Removing a Stack message from the eventQueue");
            SPDLOG_DEBUG("Calling the execute(msg) of the stateMachine");
            _stateMachine->execute(eventMsg->payload);
        }
        else if (eventMsg->evType == EventType::TimerExpired)
        {
            SPDLOG_DEBUG("");
            SPDLOG_DEBUG("****** Starting message processing in the session *********");
            SPDLOG_DEBUG("Removing a TimerExpired message from the eventQueue");  
            SPDLOG_DEBUG("Calling the timerExpired() of the stateMachine");        
            _stateMachine->timerExpired();
        }
        else if (eventMsg->evType == EventType::ExecuteAgain)
        {
            SPDLOG_DEBUG("");
            SPDLOG_DEBUG("****** Starting message processing in the session *********");
            SPDLOG_DEBUG("Removing a ExecuteAgain message from the eventQueue");
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