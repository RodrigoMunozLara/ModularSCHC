#include <cstdint>
#include <vector>
#include "spdlog/spdlog.h"
#include "SCHC_Packet.hpp"

void BitWriter::write_bits(uint32_t value, uint8_t nbits){
        if(!nbits) {
            return;
        }
        else if (nbits>32){
                spdlog::error("write_bits: nbits > 32 no soportado");
                throw std::invalid_argument("write_bits: nbits > 32 no soportado");
        }

        for(uint8_t i = 0; i< nbits ; i++){ //to write de value in bigEndian
            uint8_t bit = (value >>(nbits - 1 - i)) & 0x01;

            uint16_t byte_index = bitlen >> 3;
            uint8_t bit_in_byte = bitlen & 0x07; //&00000111
        

            if(byte_index >= buffer.size()){
                buffer.push_back(0);
            }
        
            uint8_t mask = static_cast<uint8_t>(0x80u >> bit_in_byte);
            if (bit){
                buffer[byte_index] |= mask;
            }
        
            bitlen++;
        }
}

const std::vector<uint8_t>& BitWriter::bytes() const { return buffer; }
uint32_t BitWriter::bitLength() const { return bitlen; }