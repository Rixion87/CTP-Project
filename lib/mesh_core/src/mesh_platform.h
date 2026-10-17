#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if defined(ARDUINO)
    #include <Arduino.h>
    inline uint32_t meshMillis() {
        return millis();
    }
    inline void meshLog(const char* s) {
        Serial.println(s);
    }
#else
    namespace meshplat {
        inline uint32_t& nowRef() { 
            static uint32_t t = 0; return t; 
        }
    }
    inline uint32_t meshMillis() { 
        return meshplat::nowRef(); 
    }

    #if defined(MESH_QUIET)
        inline void meshLog(const char*) {}

    #else
        inline void meshLog(const char* s) { 
            puts(s); 
        }
    #endif
#endif