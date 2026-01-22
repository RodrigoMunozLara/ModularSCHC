#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip6.h>
#include "SCHC_Node_Fragmenter.hpp"

SCHC_Node_Fragmenter frag;

int main()
{
    //int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_IPV6);
    //int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    int sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    std::cout << "Listening for IPv6 packets..." << std::endl;

    char buffer[65536];

    while (true) {
        ssize_t len = recv(sockfd, buffer, sizeof(buffer), 0);
        if (len < 0) {
            perror("recv");
            break;
        }

        frag.initialize(SCHC_FRAG_LORAWAN);
        frag.send(buffer, len);



/*

        struct ip6_hdr *ip6 = reinterpret_cast<struct ip6_hdr *>(buffer);

        char src_addr[INET6_ADDRSTRLEN];
        char dst_addr[INET6_ADDRSTRLEN];

        inet_ntop(AF_INET6, &ip6->ip6_src, src_addr, sizeof(src_addr));
        inet_ntop(AF_INET6, &ip6->ip6_dst, dst_addr, sizeof(dst_addr));

        std::cout << "---------------------------------" << std::endl;
        std::cout << "Paquete IPv6 recibido" << std::endl;
        std::cout << "Origen      : " << src_addr << std::endl;
        std::cout << "Destino     : " << dst_addr << std::endl;
        std::cout << "Next Header : " << static_cast<int>(ip6->ip6_nxt) << std::endl;
        std::cout << "Hop Limit   : " << static_cast<int>(ip6->ip6_hlim) << std::endl;
        std::cout << "Payload Len : " << ntohs(ip6->ip6_plen) << std::endl;
*/

    }

    close(sockfd);
    return 0;
}
