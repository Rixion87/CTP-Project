/*radioSerial.h*/

#pragma once
#include "radioHAL.h"
#include <variant.h>


class SerialRadio : public RadioHAL {
    public: 
        bool begin() override {
            Serial1.begin(115200); /*uses hardware UART, not USB*/
            return true; 
    }

    bool send(const uint8_t* data, uint8_t len) override {
        Serial1.write(0xAA);    /*frame start marker*/
        Serial1.write(len);
        Serial1.write(data, len);
        Serial1.write(0x55);    /*frame end marker*/
        return true;
    }

    int16_t receive(uint8_t* buf, uint8_t maxLen) override {
        if (Serial1.available() < 3); return -1;
        if (Serial1.read() != 0xAA) return -1;
        uint8_t len = Serial1.read();
        if (len > maxLen) return -1;
        Serial1.readBytes(buf, len);
        Serial1.read();        /*consume end marker*/
        return len;
    }

    int16_t getRSSI() override { return -70;} /*fake values as placeholder for now*/
    float getSNR() override {return 9.0f; }
    void setTxPower(uint8_t dbm) override {}
};