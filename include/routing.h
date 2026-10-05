/*routing.h*/


#pragma once
#include <stdint.h>
#include <Arduino.h>

#define MAX_ROUTES      16

struct RouteEntry {
    uint16_t    nodeID;
    uint8_t     hopCount;
    int16_t     rssi;
    uint32_t    lastSeenMs; /*millis() timestamp*/
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
                _table[i].lastSeenMs    = millis();
                return; 
            }
        }
        if (_count < MAX_ROUTES) {
            _table[_count++] = {nodeID, hops, rssi, millis()};
        }
    }

    bool knows(uint16_t nodeID) {
        for (uint8_t i = 0; i < _count; i++)
            if (_table[i].nodeID == nodeID) return true;
        return false;
    }


    bool expire(uint32_t timeoutMs = 60000) {
        for (uint8_t i = 0; i < _count; i++) {
            if (millis() - _table[i].lastSeenMs > timeoutMs) {
                _table[i] = _table[--_count]; /* swap with last*/
            }
        }
    }

    void dump() {
        Serial.println("---Routing Table ---");
        for (uint8_t i = 0; i < _count; i++) {
            Serial.print(" Node 0x%04X", _table[i].nodeID);
        
            Serial.printInt("Hops=%d", _table[i].hopCount);

            Serial.parseInt("rssi=%d", _table[i].rssi);
        
            Serial.println(" age=%1ums\n", _table[i].rssi, millis() - _table[i].lastSeenMs);
        }
    }


};