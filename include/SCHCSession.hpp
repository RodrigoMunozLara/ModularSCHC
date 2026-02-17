#pragma once


#include "Types.hpp"
#include "ISCHCStateMachine.hpp"
#include "ConfigStructs.hpp"
#include "SCHCAckOnErrorSender.hpp"
#include "SCHCAckOnErrorReceiver.hpp"

#include <memory>
#include <atomic>
#include "spdlog/spdlog.h"
#include <spdlog/details/os.h>

// forward declaration
class SCHCCore;


class SCHCSession
{
    public:
        SCHCSession(uint8_t id, SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore);
        ~SCHCSession();
        void        init();
        void        enqueueEvent(std::unique_ptr<EventMessage> evMsg);
        void        processEventLoop();
        void        release();
        uint8_t     getId() const { return _id; }
        bool        getDead() const { return _isDead.load(); }
        void        setDead() {_isDead.store(true); }
        void        setDevId(std::string devId) {_dev_id = devId;}
        std::string getDevId() {return _dev_id;}
    private:
        
        uint8_t         _id;   
        SCHCFragDir     _dir; 
        AppConfig       _appConfig;
        SCHCCore&       _schcCore;
        std::unique_ptr<ISCHCStateMachine>  _stateMachine;
        std::atomic<bool> running{false};
        std::atomic<bool> _isDead{false};

        std::thread processThread;

        // Queue for events
        std::queue<std::unique_ptr<EventMessage>> eventQueue;
        std::mutex eventMtx;
        std::condition_variable eventCv;

        std::string _dev_id;

};