
# Modular SCHC: Compression

This section implements the SCHC Compression mechanism in C++. The system parses network packets, selecting a matching SCHC Rule and generates the corresponding SCHC Packet, composed of:
 > SCHC Packet= Rule ID + Compression Residue

Decompression of SCHC Packets are not implemented yet.
## Core Components
- PacketParser: Extracts fields from IPv6/UDP packets
- SCHC_Rule: represents a SCHC Compression Rule containing the list of corresponding field descriptors.
    
    + Field ID
    + Field Length
    + Field Position
    + Direction Indicator
    + Target Value
    + Marching Operator
    + Compression/Decompression Action
- SCHC_RuleManager: Loads and manages available rules from configuration JSON file. Validates the format in the SCHC YANG model [RFC9363](https://www.rfc-editor.org/rfc/rfc9363.html#name-compression-rules).
- SCHC_Compressor: A state machine that perfomrs the compression process and generate the SCHC_Packet.
- SCHC_Packet: Constructs the final compressed packet output.

--- 
## Build
This project is compatible with C++17 and above, it uses CMake for build the executable and linux for compiling. Before compiling, must install:
+ CMake >= 3.10
+   Libyang development libraries ([](https://github.com/CESNET/libyang))

--- 
## Compression Test
The config folder contains the JSON file of the rules. The rule with ID: 100 match the network packet in the demo1.txt file in the packets folder. If wanna test other packets, must create another rule that matches the values of the packet.

After compiling the project, execute the CompressionTest file with a linux terminal, it will display a simple menu with the following options:
- Print Rules loaded
- Create Rule
- Start Compression
- Close Program

The process of the compression and the resulting package will be displayed in the logs.txt file.
