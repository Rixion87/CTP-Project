/*nodeID.h */
/* Responsible for node ID type, broadcast address, and how the ID is obtained per platform */

#pragma once
#include <stdint.h>

uint16_t getNodeID() {
    /*SAMD21 unqiue serial: 4 32-bit words at fixed addresses*/
    volatile uint32_t* uid = (volatile uint32_t*)0x0080A00C;
    uint32_t hash = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];
    /* XOR fold to 16 bits - stays unique in small networks*/
    return (uint16_t)(hash ^ (hash >> 16));
}