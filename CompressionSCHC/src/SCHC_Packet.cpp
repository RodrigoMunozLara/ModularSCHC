#include <cstdint>
#include <vector>
#include <stdexcept>
#include "spdlog/spdlog.h"
#include "spdlog/fmt/bin_to_hex.h" 
#include "SCHC_Packet.hpp"


void BitWriter::ensure_capacity_bits(uint32_t more_bits) {
    uint32_t total_bits = bitlen + more_bits;
    size_t need_bytes = (static_cast<size_t>(total_bits) + 7u) / 8u;
    if (buffer.size() < need_bytes) buffer.resize(need_bytes, 0);
}

inline void BitWriter::write_one_bit(uint8_t bit) {
    // destino
    size_t bi = byte_index();
    uint32_t off = bit_offset_in_byte(); // 0..7

    // off=0 -> bit 7 (MSB); off=7 -> bit 0 (LSB)
    uint8_t mask = static_cast<uint8_t>(0x80u >> off);
    if (bit & 1u) buffer[bi] |= mask;

    ++bitlen;
}

void BitWriter::pad_to_byte() {
    uint32_t rem = bitlen & 7u;
    if (rem == 0u) return;
    uint32_t to_add = 8u - rem;
    ensure_capacity_bits(to_add);
    for (uint32_t i = 0; i < to_add; ++i) write_one_bit(0);
}

void BitWriter::write_u64(uint64_t value, uint32_t nbits) {
    if (nbits == 0u) return;
    if (nbits > 64u) throw std::invalid_argument("write_u64: nbits > 64");

    ensure_capacity_bits(nbits);

    // MSB-first de esos nbits
    for (uint32_t i = 0; i < nbits; ++i) {
        uint32_t shift = (nbits - 1u - i);
        uint8_t bit = static_cast<uint8_t>((value >> shift) & 1ull);
        write_one_bit(bit);
    }
}

void BitWriter::write_from_msb_buffer(const uint8_t* data, size_t data_len, uint32_t nbits) {
    if (nbits == 0u) return;
    if (!data) throw std::invalid_argument("write_from_msb_buffer: data null");

    size_t need_src_bytes = (static_cast<size_t>(nbits) + 7u) / 8u;
    if (data_len < need_src_bytes) {
        throw std::invalid_argument("write_from_msb_buffer: bytes insuficientes para nbits");
    }

    ensure_capacity_bits(nbits);

    // FAST PATH: writer alineado + nbits múltiplo de 8 => copia directa
    if (((bitlen & 7u) == 0u) && ((nbits & 7u) == 0u)) {
        size_t dst = byte_index();
        size_t nbytes = static_cast<size_t>(nbits / 8u);
        for (size_t i = 0; i < nbytes; ++i) buffer[dst + i] = data[i];
        bitlen += nbits;
        return;
    }

    // General: bit-a-bit (correcto para cualquier alineación y nbits grandes)
    for (uint32_t i = 0; i < nbits; ++i) {
        uint32_t src_bit = i;
        uint8_t src_byte = data[src_bit / 8u];
        uint8_t src_shift = static_cast<uint8_t>(7u - (src_bit % 8u));
        uint8_t bit = static_cast<uint8_t>((src_byte >> src_shift) & 1u);
        write_one_bit(bit);
    }
}

void BitWriter::write_msb(const std::vector<uint8_t>& data, uint32_t nbits) {
    write_from_msb_buffer(data.data(), data.size(), nbits);
}

void BitWriter::write_msb(const uint8_t* data, size_t data_len, uint32_t nbits) {
    write_from_msb_buffer(data, data_len, nbits);
}

void BitWriter::write_bytes_aligned(const std::vector<uint8_t>& data) {
    write_bytes_aligned(data.data(), data.size());
}

void BitWriter::write_bytes_aligned(const uint8_t* data, size_t len) {
    if (!data && len != 0) throw std::invalid_argument("write_bytes_aligned: data null");
    if (len == 0) return;

    if ((bitlen & 7u) != 0u) {
        // Esta función es explícitamente “fast path”.
        // Si quieres permitirlo igual, llama a write_msb(data, len, len*8).
        throw std::invalid_argument("write_bytes_aligned: BitWriter no está alineado a byte");
    }

    ensure_capacity_bits(static_cast<uint32_t>(len * 8u));

    size_t dst = byte_index();
    for (size_t i = 0; i < len; ++i) buffer[dst + i] = data[i];
    bitlen += static_cast<uint32_t>(len * 8u);
}

void SCHC_Compressed_Packet::setPacketData(uint8_t rule, BitWriter& residue) {
    compressionRuleID = rule;
    compressionResidue = residue;
}   

void SCHC_Compressed_Packet::setPacketRaw(BitWriter& raw) {
    packetRaw = raw;
}  
BitWriter SCHC_Compressed_Packet::getPacketRaw() const {
    return packetRaw;
}   
void SCHC_Compressed_Packet::printPacket() const {
    spdlog::info("SCHC Packet - RuleID: {}, Residue ({} bits): {} ",
                 compressionRuleID, compressionResidue.bitLength(), spdlog::to_hex(compressionResidue.bytes()));
}

