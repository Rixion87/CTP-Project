#pragma once


#include <stdint.h>

#if defined(ARDUINO)
    #include <Arduino.h>
    inline uint32_t meshMillis() {
        return millis();
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
#endif