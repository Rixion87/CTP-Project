#include <Arduino.h>
#include <MKRWAN.h>

LoRaModem modem;

const char* devAddr = "AABBCCEE";   // different address, same keys
const char* nwkSKey = "00000000000000000000000000000001";
const char* appSKey = "00000000000000000000000000000001";

void setup() {
    Serial.begin(115200);
    while (!Serial);

    if (!modem.begin(AU915)) {
        Serial.println("Modem init failed");
        while (true);
    }

    if (!modem.joinABP(devAddr, nwkSKey, appSKey)) {
        Serial.println("ABP join failed");
        while (true);
    }

    modem.dataRate(3);
    Serial.println("Listening...");
}

void loop() {
    // Poll for received packets
    if (!modem.available()) {
        delay(100);
        return;
    }

    String received = "";
    while (modem.available()) {
        received += (char)modem.read();
    }

    Serial.print("[RX] ");
    Serial.print(received);
    //Serial.print("  RSSI: ");
    //Serial.println(modem.rssi());
}