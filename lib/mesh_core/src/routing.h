/*routing.h*/


#pragma once
#include <stdint.h>
#include "mesh_platform.h"

#define MAX_ROUTES      16

struct RouteEntry {
    uint16_t    nodeID;
    uint8_t     hopCount;
    int16_t     rssi;
    uint32_t    lastSeenMs; /*meshMillis() timestamp*/
};

class RoutingTable {
    RouteEntry  _table[MAX_ROUTES];
    uint8_t     _count = 0;

public: 
    void update(uint16_t nodeID, uint8_t hops, int16_t rssi) {
        for (uint8_t i = 0; i < _count; i++) {
            if (_table[i].nodeID == nodeID) {
                _table[i].hopCount      = hops;
                _table[i].rssi          = rssi;
                _table[i].lastSeenMs    = meshMillis();
                return; 
            }
        }
        if (_count < MAX_ROUTES) {
            _table[_count++] = {nodeID, hops, rssi, meshMillis()};
        }
    }

    bool knows(uint16_t nodeID) {
        for (uint8_t i = 0; i < _count; i++)
            if (_table[i].nodeID == nodeID) return true;
        return false;
    }


    bool expire(uint32_t timeoutMs = 60000) {
        for (uint8_t i = 0; i < _count; i++) {
            if (meshMillis() - _table[i].lastSeenMs > timeoutMs) {
                _table[i] = _table[--_count]; /* swap with last*/
            }
        }
    }

    void dump() {
        Serial.println("---Routing Table ---");
        char buf[64];
        for (uint8_t i = 0; i < _count; i++) {
            snprintf(buf, sizeof(buf),
                "Node 0x%04X Hops=%d rssi=%d age=%1ums", 
                _table[i].nodeID,
                _table[i].hopCount,
                _table[i].rssi,
                meshMillis() - _table[i].lastSeenMs);
            Serial.println(buf);
        }
    }


};