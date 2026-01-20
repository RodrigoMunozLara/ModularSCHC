
#include <cstdint>

#define MAX_RULES 6 // Maximum number of SCHC rules
static const uint8_t internalZero = 0; // Internal zero value for fields that are not sent
#define ZERO_POINTER ((uint8_t*)&internalZero) // Macro to represent a zero value pointer