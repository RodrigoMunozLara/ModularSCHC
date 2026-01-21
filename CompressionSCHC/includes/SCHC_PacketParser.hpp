#include <cstdint>
#include <array>
#include <string>
#include <vector>
#pragma once

//+-------------- Structs for raw packet headers -----------------------+
struct IPv6HeaderRaw {
    std::array<uint8_t, 4>  ver_tc_fl;     // 4 bytes: Version+TC+FlowLabel
    std::array<uint8_t, 2>  payload_len;   // 2 bytes BE
    uint8_t next_header;
    uint8_t hop_limit;
    std::array<uint8_t,16> src;
    std::array<uint8_t,16> dst;
};

struct UDPHeaderRaw {
    std::array<uint8_t,2> src_port;  // BE
    std::array<uint8_t,2> dst_port;  // BE
    std::array<uint8_t,2> length;    // BE
    std::array<uint8_t,2> checksum;  // BE
};

struct IPv6UDPFrameRaw
{
    IPv6HeaderRaw ipv6_header;
    UDPHeaderRaw udp_header;
    std::vector<uint8_t> payload; // Flexible array member for payload data
};


//+--------------Functions for parsing raw packet header------------------+

std::vector<uint8_t> hex_to_bytes_one_line(const std::string& s);


IPv6UDPFrameRaw parseIPv6UdpRaw(const std::vector<uint8_t>& b);


void dumpIPv6UdpFrame(const IPv6UDPFrameRaw& f);
