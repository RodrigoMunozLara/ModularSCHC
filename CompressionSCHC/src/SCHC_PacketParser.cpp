#include "PacketParser.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>  // std::copy
#include <stdexcept>
#include <fstream>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>

// Funciones para parsear headers de paquetes IPv6+UDP
static uint16_t payloadCut(const std::array<uint8_t,2>& x) {
    return (uint16_t(x[0]) << 8) | uint16_t(x[1]);
}

static inline uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return uint8_t(c - '0');
    if (c >= 'a' && c <= 'f') return uint8_t(10 + (c - 'a'));
    if (c >= 'A' && c <= 'F') return uint8_t(10 + (c - 'A'));

    spdlog::error("hex_nibble(): caracter invalido '{}'", c);
    throw std::runtime_error("hex invalido");
}

 std::vector<uint8_t> hex_to_bytes_one_line(const std::string& s) {
    std::vector<char> compact;
    compact.reserve(s.size());

    for (char c : s) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            compact.push_back(c);
    }

    if (compact.empty()) {
        spdlog::warn("hex_to_bytes_one_line(): linea vacia");
        return {};
    }

    if (compact.size() % 2 != 0) {
        spdlog::error("hex_to_bytes_one_line(): largo impar ({} chars)", compact.size());
        spdlog::debug("Linea original: '{}'", s);
        throw std::runtime_error("hex con largo impar");
    }

    std::vector<uint8_t> out;
    out.reserve(compact.size() / 2);

    for (size_t i = 0; i < compact.size(); i += 2) {
        try {
            uint8_t byte = (hex_nibble(compact[i]) << 4) | hex_nibble(compact[i + 1]);
            out.push_back(byte);
        } catch (...) {
            spdlog::error("hex_to_bytes_one_line(): error en pos {} ('{}{}')",
                          i, compact[i], compact[i + 1]);
            throw;
        }
    }

    spdlog::debug("hex_to_bytes_one_line(): {} bytes parseados", out.size());
    return out;
}

// ---- Parser ----
 IPv6UDPFrameRaw parseIPv6UdpRaw(const std::vector<uint8_t>& b) {
    spdlog::debug("parseIPv6UdpRaw(): input bytes={}", b.size());

    if (b.size() < 48) {
        spdlog::error("parseIPv6UdpRaw(): trama muy corta ({} bytes). Minimo 48.", b.size());
        throw std::runtime_error("Trama muy corta para IPv6+UDP");
    }

    IPv6UDPFrameRaw f{};

    // IPv6 header (40 bytes)
    std::copy(b.begin() + 0,  b.begin() + 4,  f.ipv6_header.ver_tc_fl.begin());
    std::copy(b.begin() + 4,  b.begin() + 6,  f.ipv6_header.payload_len.begin());
    f.ipv6_header.next_header = b[6];
    f.ipv6_header.hop_limit   = b[7];
    std::copy(b.begin() + 8,  b.begin() + 24, f.ipv6_header.src.begin());
    std::copy(b.begin() + 24, b.begin() + 40, f.ipv6_header.dst.begin());

    uint8_t version = (f.ipv6_header.ver_tc_fl[0] >> 4) & 0x0F;
    spdlog::debug("IPv6: version={} next_header={} hop_limit={}",
                  int(version), int(f.ipv6_header.next_header), int(f.ipv6_header.hop_limit));

    if (version != 6) {
        spdlog::error("parseIPv6UdpRaw(): No es IPv6 (version={})", int(version));
        throw std::runtime_error("No es IPv6 (version != 6)");
    }
    if (f.ipv6_header.next_header != 17) {
        spdlog::error("parseIPv6UdpRaw(): No es UDP (NextHeader={})", int(f.ipv6_header.next_header));
        throw std::runtime_error("No es UDP (Next Header != 17)");
    }
    // UDP header (8 bytes) + payload
    std::copy(b.begin() + 40, b.begin() + 42, f.udp_header.src_port.begin());
    std::copy(b.begin() + 42, b.begin() + 44, f.udp_header.dst_port.begin());
    std::copy(b.begin() + 44, b.begin() + 46, f.udp_header.length.begin());
    std::copy(b.begin() + 46, b.begin() + 48, f.udp_header.checksum.begin());

    uint16_t srcPort = payloadCut(f.udp_header.src_port);
    uint16_t dstPort = payloadCut(f.udp_header.dst_port);
    uint16_t udpLen  = payloadCut(f.udp_header.length);

    spdlog::debug("UDP: src_port={} dst_port={} length={} checksum=0x{:04x}",
                  srcPort, dstPort, udpLen, payloadCut(f.udp_header.checksum));

    if (udpLen < 8) {
        spdlog::error("parseIPv6UdpRaw(): UDP length invalido ({})", udpLen);
        throw std::runtime_error("UDP length invalido");
    }

    size_t payloadLen = size_t(udpLen) - 8;

    if (48 + payloadLen > b.size()) {
        spdlog::error("parseIPv6UdpRaw(): trama truncada. udp_payload={} bytes, disponibles={} bytes",
                      payloadLen, (b.size() - 48));
        throw std::runtime_error("Trama truncada (UDP length > bytes)");
    }

    f.payload.assign(b.begin() + 48, b.begin() + 48 + payloadLen);
    spdlog::debug("parseIPv6UdpRaw(): payload bytes={}", f.payload.size());

    return f;
}

static std::string bytes_to_hex(const uint8_t* b, size_t n) {
    std::ostringstream oss;
    for (size_t i = 0; i < n; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b[i]);
        if (i + 1 < n) oss << ":";
    }
    return oss.str();
}

void dumpIPv6UdpFrame(const IPv6UDPFrameRaw& f) {
    uint16_t srcPort =
        (uint16_t(f.udp_header.src_port[0]) << 8) | f.udp_header.src_port[1];
    uint16_t dstPort =
        (uint16_t(f.udp_header.dst_port[0]) << 8) | f.udp_header.dst_port[1];
    uint16_t udpLen =
        (uint16_t(f.udp_header.length[0]) << 8) | f.udp_header.length[1];

    spdlog::info("========== IPv6 / UDP PACKET ==========");
    spdlog::info("IPv6:");
    spdlog::info("  Version            : {}", (f.ipv6_header.ver_tc_fl[0] >> 4) & 0x0F);
    spdlog::info("  Payload Length     : {}",
                 (uint16_t(f.ipv6_header.payload_len[0]) << 8) |
                  f.ipv6_header.payload_len[1]);
    spdlog::info("  Next Header        : {}", int(f.ipv6_header.next_header));
    spdlog::info("  Hop Limit          : {}", int(f.ipv6_header.hop_limit));
    spdlog::info("  Src Address        : {}",
                 bytes_to_hex(f.ipv6_header.src.data(), 16));
    spdlog::info("  Dst Address        : {}",
                 bytes_to_hex(f.ipv6_header.dst.data(), 16));

    spdlog::info("UDP:");
    spdlog::info("  Src Port           : {}", srcPort);
    spdlog::info("  Dst Port           : {}", dstPort);
    spdlog::info("  Length             : {}", udpLen);
    spdlog::info("  Checksum           : 0x{:04x}",
                 (uint16_t(f.udp_header.checksum[0]) << 8) |
                  f.udp_header.checksum[1]);

    spdlog::info("Payload:");
    spdlog::info("  Bytes              : {}", f.payload.size());
    if (!f.payload.empty()) {
        spdlog::info("  Data               : {}",
                     bytes_to_hex(f.payload.data(), f.payload.size()));
    }

    spdlog::info("=======================================");
}


void process_hex_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        spdlog::error("No se pudo abrir archivo de tramas: '{}'", path);
        std::cout << "No se pudo abrir: " << path << "\n";
        return; // vuelve al menú, no mata el programa
    }

    std::string line;
    size_t lineNum = 0;
    size_t ok = 0, fail = 0;

    spdlog::info("Procesando archivo de tramas: '{}'", path);

    while (std::getline(in, line)) {
        ++lineNum;

        // ignora líneas vacías o comentarios
        if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0) continue;

        try {
            auto bytes = hex_to_bytes_one_line(line);
            if (bytes.empty()) continue;

            auto frame = parseIPv6UdpRaw(bytes);

            spdlog::info("OK linea {}: nextHeader={} payload_bytes={}",
                         lineNum, int(frame.ipv6_header.next_header), frame.payload.size());
            ++ok;
        } catch (const spdlog::spdlog_ex& ex) {
            spdlog::error("Error linea {}: {}", lineNum, ex.what());
            ++fail;
        }
    }

    spdlog::info("Fin procesamiento '{}': ok={} fail={}", path, ok, fail);
    std::cout << "Procesamiento terminado: ok=" << ok << " fail=" << fail << "\n";

    auto bytes = hex_to_bytes_one_line(line);
    auto frame = parseIPv6UdpRaw(bytes);

    // Mostrar paquete parseado
    dumpIPv6UdpFrame(frame);

}
