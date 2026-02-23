#ifndef SCHC_PACKET_HPP
#define SCHC_PACKET_HPP

/*SCHC Packet Definition
This header file defines the structures and enumerations necessary for representing SCHC (Static Context Header Compression) packets.
*/


#include <cstdint>
#include <vector>
#include <stdexcept>
#include <type_traits> //permite verificar el tipo de dato en tiempo de compilación

//+------------------------------------------------------------------------------------------------------+
class BitWriter {
    private:
        std::vector<uint8_t> buffer;
        uint32_t bitlen = 0; // bits escritos

        // helpers
        inline uint32_t bit_offset_in_byte() const { return bitlen & 7u; } 
        inline size_t   byte_index() const { return static_cast<size_t>(bitlen >> 3); }

        void ensure_capacity_bits(uint32_t more_bits);

        inline void write_one_bit(uint8_t bit);

        void write_from_msb_buffer(const uint8_t* data, size_t data_len, uint32_t nbits);

    public:
        BitWriter() = default; //Default Constructor

        //Getters
        const std::vector<uint8_t>& bytes() const { return buffer; }
        uint32_t bitLength() const { return bitlen; }
        void clear() { buffer.clear(); bitlen = 0; }

        // padding a byte with 0
        void pad_to_byte();

        // Writer for FIXED length fields 
        void write_u64(uint64_t value, uint32_t nbits);

        template <typename T>
        void write_uint(T value, uint32_t nbits) {
            static_assert(std::is_integral<T>::value, "write_uint: T debe ser integral");
            static_assert(std::is_unsigned<T>::value, "write_uint: T debe ser unsigned");
            if (nbits > sizeof(T) * 8u) {
                throw std::invalid_argument("write_uint: nbits excede tamaño del tipo");
            }
            write_u64(static_cast<uint64_t>(value), nbits);
        }

        //Writer for vector of bytes
        void write_msb(const std::vector<uint8_t>& data, uint32_t nbits);
        void write_msb(const uint8_t* data, size_t data_len, uint32_t nbits);

        //Writer if the vector is already MSB-aligned
        void write_bytes_aligned(const std::vector<uint8_t>& data);
        void write_bytes_aligned(const uint8_t* data, size_t len);

        // Convenience: escribir otro BitWriter completo
        void write_writer(const BitWriter& other) {
            write_msb(other.bytes(), other.bitLength());
        }
};



class SCHC_Compressed_Packet {
    private:
        uint8_t compressionRuleID; //Compression Rule ID. 1 byte
        BitWriter compressionResidue; //Compression Residue. 4 bytes for item. Dinamic size
        BitWriter packetRaw; //Raw packet with RuleID + CompressionResidue

    public:
        SCHC_Compressed_Packet() = default; //Default Constructor
        ~SCHC_Compressed_Packet() = default; //Destructor

        uint32_t getPacketLength(); //Getter for packet length

        void setPacketData(uint8_t rule, BitWriter& residue); //Setter for packet data and length
        
        void setPacketRaw(BitWriter& raw); //Setter for raw packet data
        BitWriter getPacketRaw() const; //Getter for raw packet data

        void printPacket() const; //Print Packet Details
};

#endif