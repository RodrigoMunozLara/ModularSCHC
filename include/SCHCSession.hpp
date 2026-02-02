#pragma once


#include "Types.hpp"
#include "ISCHCStateMachine.hpp"
#include "ConfigStructs.hpp"
#include "SCHCAckOnErrorSender.hpp"

#include <memory>
#include <atomic>
#include "spdlog/spdlog.h"

// forward declaration
class SCHCCore;


class SCHCSession
{
    public:
        SCHCSession(uint32_t id, SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore);
        void        startCompression(char *buffer, int len);
        void        startFragmentation(char *buffer, int len);
        bool        isAssigned() const { return _assigned; }
        void        assign() { _assigned = true; }
        void        release();
        uint32_t    getId() const { return _id; }
    private:
        bool _assigned = false;
        
        uint32_t        _id;   
        SCHCFragDir     _dir; 
        AppConfig       _appConfig;
        SCHCCore&       _schcCore;
        std::unique_ptr<ISCHCStateMachine>  _stateMachine;
        std::atomic<bool> running{false};

};