#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <fstream>
#include "SCHC_Rule.hpp"
#include "PacketParser.hpp"

static inline uint16_t read_u16_be(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static inline uint32_t read_u32_be(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
           (static_cast<uint32_t>(p[3]));
}

// Extract nbits starting at bit offset 'bit_off' from a big-endian buffer.
// Returns packed bytes big-endian (minimal bytes), and sets bit_length.
static std::vector<uint8_t> extract_bits_be(const uint8_t* data, size_t data_len,
                                            size_t bit_off, uint16_t nbits)
{
    if (nbits == 0) return {};
    size_t total_bits = data_len * 8;
    if (bit_off + nbits > total_bits) {
        throw std::out_of_range("extract_bits_be: out of range");
    }

    size_t out_bytes = (nbits + 7) / 8;
    std::vector<uint8_t> out(out_bytes, 0);

    for (uint16_t i = 0; i < nbits; ++i) {
        size_t src_bit = bit_off + i;
        uint8_t src_byte = data[src_bit / 8];
        uint8_t src_bit_in_byte = static_cast<uint8_t>(7 - (src_bit % 8));
        uint8_t bit = (src_byte >> src_bit_in_byte) & 0x01;

        size_t dst_bit = i;
        size_t dst_byte_idx = dst_bit / 8;
        uint8_t dst_bit_in_byte = static_cast<uint8_t>(7 - (dst_bit % 8));
        out[dst_byte_idx] |= static_cast<uint8_t>(bit << dst_bit_in_byte);
    }
    return out;
}

static void push_field(std::vector<FieldValue>& out,
                       const std::string& fid,
                       const std::vector<uint8_t>& val,
                       uint16_t bitlen)
{
    FieldValue f;
    f.fid = fid;
    f.value = val;
    f.bit_length = bitlen;
    out.push_back(std::move(f));
}

// -------------------- main parser: IPv6 + UDP --------------------
// direction_uplink = true means packet is device->app (uplink).
// That decides how you populate fid-ipv6-devprefix vs fid-ipv6-appprefix, etc.
std::vector<FieldValue> parse_ipv6_udp_fields(const std::vector<uint8_t>& pkt,
                                              bool direction_uplink)
{
    std::vector<FieldValue> out; 
    // Minimal IPv6 header = 40 bytes
    if (pkt.size() < 40) throw std::invalid_argument("Packet too short for IPv6");

    const uint8_t* ip = pkt.data();

    // First 4 bytes: Version (4), Traffic Class (8), Flow Label (20)
    // Layout: [0..31] big-endian bits
    // version: bits 0..3
    // traffic class: bits 4..11
    // flow label: bits 12..31
    auto v  = extract_bits_be(ip, 4, 0, 4);
    auto tc = extract_bits_be(ip, 4, 4, 8);
    auto fl = extract_bits_be(ip, 4, 12, 20);

    push_field(out, "ietf-schc:fid-ipv6-version",       v,  4);
    push_field(out, "ietf-schc:fid-ipv6-trafficclass",  tc, 8);
    push_field(out, "ietf-schc:fid-ipv6-flowlabel",     fl, 20);

    // Payload Length (16) at bytes 4..5
    uint16_t payload_len = read_u16_be(ip + 4);
    push_field(out, "ietf-schc:fid-ipv6-payload-length",
               { static_cast<uint8_t>(payload_len >> 8),
                 static_cast<uint8_t>(payload_len & 0xFF) }, 16);

    // Next Header (8) at byte 6
    push_field(out, "ietf-schc:fid-ipv6-nextheader", { ip[6] }, 8);

    // Hop Limit (8) at byte 7
    push_field(out, "ietf-schc:fid-ipv6-hoplimit", { ip[7] }, 8);

    // Src addr (16 bytes) at 8..23, Dst addr at 24..39
    const uint8_t* src = ip + 8;
    const uint8_t* dst = ip + 24;

    std::vector<uint8_t> src_prefix(src, src + 8);
    std::vector<uint8_t> src_iid   (src + 8, src + 16);
    std::vector<uint8_t> dst_prefix(dst, dst + 8);
    std::vector<uint8_t> dst_iid   (dst + 8, dst + 16);

    // Map to dev/app depending on direction
    // Uplink (device->app): dev = src, app = dst
    // Downlink (app->device): dev = dst, app = src
    if (direction_uplink) {
        push_field(out, "ietf-schc:fid-ipv6-devprefix", src_prefix, 64);
        push_field(out, "ietf-schc:fid-ipv6-deviid",    src_iid,    64);
        push_field(out, "ietf-schc:fid-ipv6-appprefix", dst_prefix, 64);
        push_field(out, "ietf-schc:fid-ipv6-appiid",    dst_iid,    64);
    } else {
        push_field(out, "ietf-schc:fid-ipv6-devprefix", dst_prefix, 64);
        push_field(out, "ietf-schc:fid-ipv6-deviid",    dst_iid,    64);
        push_field(out, "ietf-schc:fid-ipv6-appprefix", src_prefix, 64);
        push_field(out, "ietf-schc:fid-ipv6-appiid",    src_iid,    64);
    }

    // UDP parsing only if Next Header == 17 and enough bytes
    if (ip[6] == 17) {
        if (pkt.size() < 40 + 8) throw std::invalid_argument("Packet too short for UDP");
        const uint8_t* udp = pkt.data() + 40;

        uint16_t src_port = read_u16_be(udp + 0);
        uint16_t dst_port = read_u16_be(udp + 2);
        uint16_t udp_len  = read_u16_be(udp + 4);
        uint16_t udp_csum = read_u16_be(udp + 6);

        // dev/app ports depend on direction (uplink: dev-port=src, app-port=dst)
        if (direction_uplink) {
            push_field(out, "ietf-schc:fid-udp-dev-port",
                       { static_cast<uint8_t>(src_port >> 8), static_cast<uint8_t>(src_port & 0xFF) }, 16);
            push_field(out, "ietf-schc:fid-udp-app-port",
                       { static_cast<uint8_t>(dst_port >> 8), static_cast<uint8_t>(dst_port & 0xFF) }, 16);
        } else {
            push_field(out, "ietf-schc:fid-udp-dev-port",
                       { static_cast<uint8_t>(dst_port >> 8), static_cast<uint8_t>(dst_port & 0xFF) }, 16);
            push_field(out, "ietf-schc:fid-udp-app-port",
                       { static_cast<uint8_t>(src_port >> 8), static_cast<uint8_t>(src_port & 0xFF) }, 16);
        }

        push_field(out, "ietf-schc:fid-udp-length",
                   { static_cast<uint8_t>(udp_len >> 8), static_cast<uint8_t>(udp_len & 0xFF) }, 16);

        push_field(out, "ietf-schc:fid-udp-checksum",
                   { static_cast<uint8_t>(udp_csum >> 8), static_cast<uint8_t>(udp_csum & 0xFF) }, 16);
    }

    return out;
}

std::string read_file_to_string(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("No pude abrir el archivo: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Convierte una cadena hex (con separadores) a bytes.
// Acepta: espacios, \n, \t, ':', '-', ',', y prefijo 0x.
std::vector<uint8_t> hex_to_bytes(std::string s) {
    // 1) Quitar "0x" o "0X"
    for (size_t pos = 0; (pos = s.find("0x")) != std::string::npos; ) s.erase(pos, 2);
    for (size_t pos = 0; (pos = s.find("0X")) != std::string::npos; ) s.erase(pos, 2);

    // 2) Filtrar solo caracteres hex
    std::string hex;
    hex.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isxdigit(c)) hex.push_back((char)c);
    }

    if (hex.size() % 2 != 0) {
        throw std::runtime_error("Hex inválido: cantidad impar de dígitos (" + std::to_string(hex.size()) + ")");
    }

    auto hexval = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
        c = (char)std::tolower((unsigned char)c);
        if (c >= 'a' && c <= 'f') return (uint8_t)(10 + (c - 'a'));
        throw std::runtime_error("Carácter no hex");
    };

    std::vector<uint8_t> out;
    out.reserve(hex.size()/2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t hi = hexval(hex[i]);
        uint8_t lo = hexval(hex[i+1]);
        out.push_back((uint8_t)((hi << 4) | lo));
    }
    return out;
}