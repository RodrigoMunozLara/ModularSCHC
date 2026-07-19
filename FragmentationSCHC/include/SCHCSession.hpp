#pragma once


#include "Types.hpp"
#include "interfaces/ISCHCStateMachine.hpp"
#include "ConfigStructs.hpp"
#include "schcAckOnError/SCHCAckOnErrorSender.hpp"
#include "schcAckOnError/SCHCAckOnErrorReceiver.hpp"
#include "schcArqFec/SCHCArqFecSender.hpp"
#include "schcArqFec/SCHCArqFecReceiver.hpp"

#include <memory>
#include <atomic>
#include "spdlog/spdlog.h"
#include <spdlog/details/os.h>
#include <iostream>
#include <filesystem>

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
        int         read_sat_pass();
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
    public:
        std::chrono::steady_clock::time_point   _startTime;
        std::vector<long long>                  _msgTimes_vector;
        std::vector<int>                        _msgTimesType_vector;
        std::vector<int>                        _visibility_col;
        std::vector<int>                        _revisit_col;
        int                                     _sat_win_ptr = 0;
        int                                     _win_elapsed = 0;
        long long                               _acumulative_win = 0;
        

};