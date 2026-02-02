#include "BackhaulCore.hpp"


BackhaulCore::BackhaulCore(Orchestrator& orch, AppConfig& appConfig): orchestrator(orch), _appConfig(appConfig)
{
    SPDLOG_INFO("Executing BackhaulCore constructor()");
}

BackhaulCore::~BackhaulCore()
{
    SPDLOG_INFO("Executing BackhaulCore destructor()");

}

CoreId BackhaulCore::id()
{
    SPDLOG_TRACE("Entering the function");
    return CoreId::BACKHAUL;
    SPDLOG_TRACE("Leaving the function");
}

void BackhaulCore::enqueueFromOrchestator(std::unique_ptr<RoutedMessage> msg)
{
}

void BackhaulCore::start()
{
    SPDLOG_TRACE("Entering the function");
    if (running.load())
    {
        SPDLOG_ERROR("The BackhaulCore::start() method cannot be called when the BackhaulCore is already running.");
        return;
    }
    else
    {
        SPDLOG_ERROR("Setting the BackhaulCore as active");
        running.store(true);
    }


    stopfd = eventfd(0, EFD_NONBLOCK);

    // name of the interface to which the socket will be associated
    const char* iface = _appConfig.backhaul.interface_name.c_str();

    // Create RAW socket for ICMPv6/IPv6 packets
    sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IPV6));
    if (sockfd < 0) {
        SPDLOG_ERROR("Failed to create raw socket: {}", strerror(errno));
        return;
    }

    // Obtain interface index
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ-1);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        SPDLOG_ERROR("Failed to obtain interface index");
        close(sockfd);
        return;
    }
    int ifindex = ifr.ifr_ifindex;
    SPDLOG_INFO("Obtained interface index '{}' with index '{}'", iface, ifindex);


    // Prepare sockaddr_ll for bind
    struct sockaddr_ll addr;
    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_IPV6);
    addr.sll_ifindex = ifindex;

    // Associate socket with interface
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        SPDLOG_ERROR("Failed to associate socket with interface");
        close(sockfd);
        return;
    }
    SPDLOG_INFO("Socket AF_PACKET associated with '{}'", iface);

    SPDLOG_INFO("Starting threads...");
    rxThread = std::thread(&BackhaulCore::runRx, this);
    txThread = std::thread(&BackhaulCore::runTx, this);
    SPDLOG_INFO("BackhaulCore::runRx thread STARTED");
    SPDLOG_INFO("BackhaulCore::runTx thread STARTED");

    SPDLOG_INFO("BackhaulCore STARTED");
    SPDLOG_TRACE("Leaving the function");
}

void BackhaulCore::stop()
{
    SPDLOG_TRACE("Entering the function");

    running.store(false);

    uint64_t one = 1;
    write(stopfd, &one, sizeof(one));  // despierta poll inmediatamente

    // Wake up TX thread
    txCv.notify_all();

    if (sockfd >= 0)
    {
        //shutdown(sockfd, SHUT_RD); // optional but recommended
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
        SPDLOG_INFO("sockfd closed");
    }

    if (rxThread.joinable()) {
        rxThread.join();
    }

    if (txThread.joinable()) {
        txThread.join();
    }

    SPDLOG_INFO("BackhaulCore stopped");
    SPDLOG_TRACE("Leaving the function");
}

void BackhaulCore::runRx()
{
    SPDLOG_TRACE("Entering the function");

    struct pollfd fds[2];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    fds[1].fd = stopfd;
    fds[1].events = POLLIN;

    while (running.load())
    {
        int ret = poll(fds, 2, -1); // bloqueante
        if (ret < 0) {
            SPDLOG_ERROR("Poll");
            break;
        }

        // Señal de salida
        if (fds[1].revents & POLLIN) {
            SPDLOG_INFO("Stop signal received");
            break;
        }

        // Paquete recibido
        if (fds[0].revents & POLLIN)
        {
            uint8_t buffer[2048];
            struct sockaddr_ll saddr;
            socklen_t saddr_len = sizeof(saddr);

            ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&saddr, &saddr_len);


            if(saddr.sll_pkttype == PACKET_OTHERHOST) 
            {

                if (!running.load())
                    break;

                if (len < 0)
                {
                    if (errno == EINTR)
                        continue;

                    SPDLOG_ERROR("recvfrom failed");
                    break;
                }

                handleRxFrame({buffer, buffer + len});
            }
        }
    }

    SPDLOG_INFO("Thread finished");
    SPDLOG_TRACE("Leaving the function");
}

void BackhaulCore::handleRxFrame(const std::vector<uint8_t>& frame)
{
    SPDLOG_TRACE("Entering the function");
    auto msg = std::make_unique<RoutedMessage>();

    // Populate minimal routing metadata
    msg->meta.ingress = CoreId::BACKHAUL;
    msg->payload = frame;

    SPDLOG_INFO("Message in hex: {:Xp}", spdlog::to_hex(frame)); 


    IPv6Addresses addrs = getIPv6Addresses(frame);
    SPDLOG_INFO("IPv6 src: {}", addrs.src);
    SPDLOG_INFO("IPv6 dst: {}", addrs.dst);



    // Forward immediately to the Orchestrator
    orchestrator.onMessageFromCore(std::move(msg));
    SPDLOG_TRACE("Leaving the function");
}

void BackhaulCore::enqueueFromStack(std::unique_ptr<RoutedMessage> msg)
{
    SPDLOG_TRACE("Entering the function");
    {
        std::lock_guard<std::mutex> lock(txMtx);
        txQueue.push(std::move(msg));
    }

    txCv.notify_one();
    SPDLOG_TRACE("Leaving the function");
}

void BackhaulCore::runTx()
{
    SPDLOG_TRACE("Entering the function");
    while (running.load()) {

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

        sendFrame(msg->payload);
    }

    SPDLOG_INFO("Thread finished");
    SPDLOG_TRACE("Leaving the function");
}

void BackhaulCore::sendFrame(const std::vector<uint8_t>& frame)
{
    SPDLOG_TRACE("Entering the function");
    ssize_t sent = send(
        sockfd,
        frame.data(),
        frame.size(),
        0
    );

    if (sent < 0)
    {
        SPDLOG_ERROR("Failed to send IPv6 frame on BackhaulCore TX");
    }
    SPDLOG_TRACE("Leaving the function");
}

IPv6Addresses BackhaulCore::getIPv6Addresses(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < sizeof(struct ip6_hdr)) {
        throw std::runtime_error("Buffer demasiado pequeño para IPv6");
    }

    const struct ip6_hdr* ip6 = reinterpret_cast<const struct ip6_hdr*>(buffer.data());

    char src_str[INET6_ADDRSTRLEN];
    char dst_str[INET6_ADDRSTRLEN];

    inet_ntop(AF_INET6, &ip6->ip6_src, src_str, sizeof(src_str));
    inet_ntop(AF_INET6, &ip6->ip6_dst, dst_str, sizeof(dst_str));

    IPv6Addresses addresses;
    addresses.src = src_str;
    addresses.dst = dst_str;

    return addresses;
}