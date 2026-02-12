#ifndef SCHC_PACKET_HPP
#define SCHC_PACKET_HPP

/*SCHC Packet Definition
This header file defines the structures and enumerations necessary for representing SCHC (Static Context Header Compression) packets.
*/


#include <cstdint>
#include <vector>
//+------------------------------------------------------------------------------------------------------+

class BitWriter {
    private:
        std::vector<uint8_t> buffer;
        uint32_t bitlen = 0;

    public:
        void write_bits(uint32_t value, uint8_t nbits);
        const std::vector<uint8_t> &bytes() const;
        uint32_t bitLength() const;
    };


class SCHC_Compressed_Packet {
    private:
        uint8_t compressionRuleID; //Compression Rule ID. 1 byte
        BitWriter compressionResidue; //Compression Residue. 4 bytes for item. Dinamic size
        uint8_t paddingBits; 
        BitWriter packetRaw; //Raw packet with RuleID + CompressionResidue

    public:
        SCHC_Compressed_Packet() = default; //Default Constructor
        ~SCHC_Compressed_Packet(); //Destructor

        uint32_t getPacketLength(); //Getter for packet length

        void setPacketData(uint8_t rule, BitWriter& residue, uint8_t padBits); //Setter for packet data and length

        void printPacket() const; //Print Packet Details
};

#endif