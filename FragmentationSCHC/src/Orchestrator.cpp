#include "Orchestrator.hpp"


Orchestrator::Orchestrator()
{
    SPDLOG_DEBUG("Executing Orchestrator constructor()");
}

Orchestrator::~Orchestrator()
{
    SPDLOG_DEBUG("Executing Orchestrator destructor()");

}

void Orchestrator::onMessageFromCore(std::unique_ptr<RoutedMessage> msg)
{
    if (!msg) {
        SPDLOG_WARN("Orchestrator received null message");
        return;
    }

    // Decisión de ruteo basada en metadata (NO modifica estado)
    CoreId dst = selectDestination(*msg);

    // Forward transfiere ownership
    forwardToCore(dst, std::move(msg));
}

void Orchestrator::registerCore(CoreId id, ICore* core)
{
    if (!core) {
        throw std::invalid_argument("Cannot register null core");
    }

    std::lock_guard<std::mutex> lock(mtx);

    auto result = cores.emplace(id, core);
    if (!result.second) {
        throw std::runtime_error("Core already registered");
    }

    SPDLOG_DEBUG("Registered Core {}", static_cast<int>(id));
}

CoreId Orchestrator::selectDestination(const RoutedMessage& msg)
{
    /*
     * EXAMPLES of possible criteria:
     *  - msg.meta.ingress
     *  - msg.meta.payloadSize
     */

    if (msg.meta.ingress == CoreId::BACKHAUL) {
        SPDLOG_DEBUG("Receiving message from BACKHAUL Core. Message routed to SCHC Core");
        return CoreId::SCHC;
    }

    if (msg.meta.ingress == CoreId::SCHC) {
        SPDLOG_DEBUG("Receiving message from SCHC Core. Message routed to BACKHAUL Core");
        return CoreId::BACKHAUL;
    }

    // Fallback explícito
    throw std::runtime_error("No routing rule for message");
}

void Orchestrator::forwardToCore(CoreId dst, std::unique_ptr<RoutedMessage> msg)
{
    ICore* target = nullptr;

    {
        std::lock_guard<std::mutex> lock(mtx);

        auto it = cores.find(dst);
        if (it == cores.end()) {
            throw std::runtime_error("Destination stack not registered");
        }

        target = it->second;
    }

    // Envío fuera del mutex (MUY IMPORTANTE)
    target->enqueueFromOrchestator(std::move(msg));

}

void Orchestrator::stop()
{
    for (auto const& [id, core] : cores) {
        if (core != nullptr) {
            SPDLOG_DEBUG("Stoping Core: {}", static_cast<int>(id));
            core->stop();
        }
    }

}

