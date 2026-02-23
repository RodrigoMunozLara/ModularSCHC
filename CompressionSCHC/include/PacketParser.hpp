#ifndef PACKETPARSER_HPP
#define PACKETPARSER_HPP


#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <algorithm>

struct FieldValue {
    std::string fid;              // e.g., "ietf-schc:fid-ipv6-version"
    std::vector<uint8_t> value;   // big-endian bytes (packed)
    uint16_t bit_length = 0;      // exact length in bits (important for SCHC)
};

// -------------------- helpers --------------------

static inline uint16_t read_u16_be(const uint8_t* p) ;

static inline uint32_t read_u32_be(const uint8_t* p) ;

// Extract nbits starting at bit offset 'bit_off' from a big-endian buffer.
// Returns packed bytes big-endian (minimal bytes), and sets bit_length.
static std::vector<uint8_t> extract_bits_be(const uint8_t* data, size_t data_len,
                                            size_t bit_off, uint16_t nbits);

static void push_field(std::vector<FieldValue>& out,
                       const std::string& fid,
                       const std::vector<uint8_t>& val,
                       uint16_t bitlen);

// -------------------- main parser: IPv6 + UDP --------------------
// direction_uplink = true means packet is device->app (uplink).
// That decides how you populate fid-ipv6-devprefix vs fid-ipv6-appprefix, etc.
std::vector<FieldValue> parse_ipv6_udp_fields(const std::vector<uint8_t>& pkt,
                                              bool direction_uplink);


std::string read_file_to_string(const std::string& path);
std::vector<uint8_t> hex_to_bytes(std::string s);

#endif