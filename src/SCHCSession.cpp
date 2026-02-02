#include "SCHCSession.hpp"

class SCHCCore; 

SCHCSession::SCHCSession(uint32_t id, SCHCFragDir dir, AppConfig& appConfig, SCHCCore& schcCore): _id(id), _dir(dir), _appConfig(appConfig), _schcCore(schcCore)
{
    SPDLOG_INFO("SCHCSession created with id '{}'", id);

    if((dir == SCHCFragDir::UPLINK_DIR) 
        && (_appConfig.schc.schc_type.compare("schc_node") == 0) 
        && (_appConfig.schc.schc_reliability.compare("false") == 0) 
        && (_appConfig.schc.schc_l2_protocol.compare("lorawan") == 0))
    {
        _stateMachine = std::make_unique<SCHCAckOnErrorSender>(dir, _appConfig, _schcCore);
    }

}

void SCHCSession::startCompression(char *buffer, int len)
{
    /* ToDo: Trabajar con Fabian Alfaro (practicante AC3E)*/
}

void SCHCSession::startFragmentation(char *buffer, int len)
{
    SPDLOG_TRACE("Entering the function");
    
    _stateMachine->start(buffer, len);
    
    while (running.load())
    {
        _stateMachine->execute();
    }
    

    SPDLOG_TRACE("Leaving the function");
}

void SCHCSession::release()
{
    SPDLOG_INFO("Releasing session resources");
    _assigned = false;
    _stateMachine->release();
    SPDLOG_INFO("Session resources released");
}
