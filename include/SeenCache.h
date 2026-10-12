/*SeenCache.h*/


#pragma once
#include <stdint.h>

#define SEEN_CACHE_SIZE 16

struct SeenEntry {
        uint16_t srcID;
        uint8_t seqNum;
    };

class SeenCache {
    SeenEntry   _cache[SEEN_CACHE_SIZE];
    uint8_t     _head = 0; /*Circular buffer head*/

public:
    bool hasSeen(uint16_t srcID, uint8_t seqNum) {
        for (uint8_t i = 0; i < SEEN_CACHE_SIZE; i++) {
        if (_cache[i].srcID == srcID && _cache[i].seqNum == seqNum) 
            return true;
        }
        return false;
    }

    void markSeen(uint16_t srcID, uint8_t seqNum) {
        _cache[_head] = {srcID, seqNum};
        _head = (_head + 1) % SEEN_CACHE_SIZE;
    }
};