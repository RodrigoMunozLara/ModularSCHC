#pragma once

#include "ICore.hpp"
#include "Orchestrator.hpp"
#include "Types.hpp"
#include "ConfigStructs.hpp"

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/icmp6.h>
#include <linux/if_ether.h>      // ETH_P_IPV6
#include <netpacket/packet.h>     // sockaddr_ll
#include <net/if.h>               // if_nametoindex
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <chrono>
#include <stdexcept>
#include <netinet/ip6.h>

struct IPv6Addresses {
    std::string src;
    std::string dst;
};


class BackhaulCore : public ICore
{
public:
    BackhaulCore(Orchestrator& orchestrator, AppConfig& appConfig);
    ~BackhaulCore() override;
    CoreId          id() override;
    void            enqueueFromOrchestator(std::unique_ptr<RoutedMessage> msg) override;
    void            start() override;
    void            stop() override;
private:
    void            runTx() override;
    void            runRx() override;
    void            runCleaner() override;
    void            handleRxFrame(const std::vector<uint8_t>& frame);
    void            sendFrame(const std::vector<uint8_t>& frame);
    IPv6Addresses   getIPv6Addresses(const std::vector<uint8_t>& buffer);

private:
    Orchestrator& orchestrator;
    std::atomic<bool> running{false};   /* Thread control flag */
    AppConfig _appConfig;               /* Configuration Struct */

    /* Worker threads */
    std::thread rxThread;
    std::thread txThread;

    /* Queue for messages from the Orchestrator */
    std::queue<std::unique_ptr<RoutedMessage>> txQueue;
    std::mutex txMtx;
    std::condition_variable txCv;

    /* IPv6 socket file descriptor */
    int sockfd{-1};
    int stopfd{-1};
};
