#include <cstdint>
#include <vector>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include "SCHC_Packet.hpp"

// write value in the size in bits given in nbits in a buffer in the struct
void BitWriter::write_bits(uint16_t value, uint8_t nbits) {
    if (!nbits) return;

    if (nbits > 16) {
        spdlog::error("write_bits: nbits > 16 no soportado");
        throw std::invalid_argument("write_bits: nbits > 16 no soportado");
    }

    for (uint8_t i = 0; i < nbits; i++) { // big-endian bit order
        uint8_t bit = (value >> (nbits - 1 - i)) & 0x01;

        uint16_t byte_index = bitlen >> 3;
        uint8_t bit_in_byte = bitlen & 0x07;

        if (byte_index >= buffer.size()) {
            buffer.push_back(0);
        }

        uint8_t mask = static_cast<uint8_t>(0x80u >> bit_in_byte);
        if (bit) {
            buffer[byte_index] |= mask;
        }

        bitlen++;
    }
}

// --- Nuevo overload: recibe bytes y nbits ---
void BitWriter::write_bits(const std::vector<uint8_t>& bytes, uint8_t nbits) {
    if (!nbits) return;

    if (nbits > 32) {
        spdlog::error("write_bits(bytes): nbits > 32 no soportado");
        throw std::invalid_argument("write_bits(bytes): nbits > 32 no soportado");
    }

    const size_t needed_bytes = (nbits + 7u) / 8u;
    if (bytes.size() < needed_bytes) {
        spdlog::error("write_bits(bytes): bytes.size()={} insuficiente para nbits={}",
                      bytes.size(), nbits);
        throw std::invalid_argument("write_bits(bytes): bytes insuficientes para nbits");
    }

    // Empaqueta en big-endian usando los primeros needed_bytes
    uint32_t value = 0;
    for (size_t i = 0; i < needed_bytes; ++i) {
        value = (value << 8) | static_cast<uint32_t>(bytes[i]);
    }

    // Si nbits no es múltiplo de 8, asumimos que el campo está MSB-alineado
    // (ej: IPv6 version está en los 4 bits altos de 0x60 -> queda 0x06)
    const uint8_t extra = static_cast<uint8_t>(needed_bytes * 8u - nbits);
    if (extra) {
        value >>= extra;
    }

    // Reusa el core
    write_bits(value, nbits);
}

const std::vector<uint8_t>& BitWriter::bytes() const { return buffer; }
uint32_t BitWriter::bitLength() const { return bitlen; }
