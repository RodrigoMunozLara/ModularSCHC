#ifndef SCHC_PACKET_HPP
#define SCHC_PACKET_HPP

/*SCHC Packet Definition
This header file defines the structures and enumerations necessary for representing SCHC (Static Context Header Compression) packets.
*/


#include <cstdint>
#include <vector>
//+------------------------------------------------------------------------------------------------------+

class SCHC_Compressed_Packet {
    private:
        uint8_t compressionRuleID; //Compression Rule ID. 1 byte
        std::vector <uint16_t> compressionResidue; //Compression Residue. 4 bytes for item. Dinamic size
        uint8_t padding; //Padding to align to byte boundary
        

    public:
        SCHC_Compressed_Packet(); //Default Constructor
        SCHC_Compressed_Packet(uint8_t rule, std::vector<uint16_t> residue); //Constructor with parameters
        ~SCHC_Compressed_Packet(); //Destructor

        uint32_t getPacketLength(); //Getter for packet length

        void setPacketData(uint8_t rule, std::vector<uint16_t> residue, uint8_t padding); //Setter for packet data and length

        void printPacket() const; //Print Packet Details
};

#endif