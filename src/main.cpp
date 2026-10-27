#include <Arduino.h>
#include <stdlib.h>
#include <radio_select.h>
#include <mesh.h>

/* Two-node text chat over the mesh.
   Open the serial monitor (115200 baud, line ending = Newline) on each board and type:
       hello              -> broadcast to every node
       @2 hello           -> send directly to node 2
       /routes            -> show the neighbour table
       /stats             -> frames heard with a bad CRC / address
       /pins              -> idle levels of the CD, DR and AM lines (should all be 0)
       /help
   NODE_ID comes from the build flags, so flash the same sketch to both boards with different IDs. */

Radio    radio;
MeshNode mesh(&radio, NODE_ID);

#define LINE_MAX 40
static char    lineBuf[LINE_MAX];
static uint8_t lineLen  = 0;
static bool    overflow = false;

static void printHelp() {
    Serial.println(F("Type a message and press Enter.  max 17 chars (nRF905 frame limit)"));
    Serial.println(F("  text        broadcast to all nodes"));
    Serial.println(F("  @N text     send directly to node N"));
    Serial.println(F("  /routes  /stats  /pins  /help"));
}

/* Called by MeshNode for every packet addressed to this node (or broadcast). */
static void onMessage(const MeshPacket& p, void*) {
    Serial.print(F("<"));
    Serial.print(p.srcID);
    Serial.print(F("> "));
    Serial.write(p.payload, p.payloadLen);
    Serial.print(F("   ("));
    Serial.print(MAX_TTL - p.ttl);
    Serial.println(F(" relays)"));
}

static void handleLine(char* line) {
    /* ---- commands ---- */
    if (line[0] == '/') {
        if (strcmp(line, "/routes") == 0) {
            if (mesh.routes().count() == 0) Serial.println(F("(no neighbours heard yet)"));
            else mesh.routes().dump();
        } else if (strcmp(line, "/pins") == 0) {
#ifdef RADIO_NRF905
            /* Idle, in receive mode, all three should read 0. */
            Serial.print(F("CD="));
            if (RADIO_PIN_CD != NRF905_PIN_UNUSED) Serial.print(digitalRead(RADIO_PIN_CD)); else Serial.print(F("n/a"));
            Serial.print(F("  DR="));
            if (RADIO_PIN_DR != NRF905_PIN_UNUSED) Serial.print(digitalRead(RADIO_PIN_DR)); else Serial.print(F("n/a"));
            Serial.print(F("  AM="));
            if (RADIO_PIN_AM != NRF905_PIN_UNUSED) Serial.println(digitalRead(RADIO_PIN_AM)); else Serial.println(F("n/a"));
#else
            Serial.println(F("(not available for this radio)"));
#endif
        } else if (strcmp(line, "/stats") == 0) {
#ifdef RADIO_NRF905
            Serial.print(F("Bad frames heard: "));
            Serial.println(radio.invalidFrames());
#else
            Serial.println(F("(no stats for this radio)"));
#endif
        } else {
            printHelp();
        }
        return;
    }

    /* ---- message, optionally prefixed with @N ---- */
    uint16_t dest = BROADCAST_ADDR;
    char*    text = line;
    if (line[0] == '@') {
        char* end;
        long  id = strtol(line + 1, &end, 10);
        if (end == line + 1 || id <= 0 || id >= 0xFFFF || *end != ' ') {
            Serial.println(F("Usage: @N message   (N = node number)"));
            return;
        }
        dest = (uint16_t)id;
        text = end;
        while (*text == ' ') text++;
        if (*text == 0) { Serial.println(F("Empty message")); return; }
    }

    size_t n = strlen(text);
    if (n > MAX_PAYLOAD_LEN) {
        Serial.print(F("Too long: "));
        Serial.print(n);
        Serial.print(F(" chars, max "));
        Serial.println(MAX_PAYLOAD_LEN);
        return;
    }

    if (mesh.sendMessage(dest, text)) { Serial.println(F("Sent")); return; }

#ifdef RADIO_NRF905
    switch (radio.lastSendStatus()) {
        case NRF905_SEND_CHANNEL_BUSY:
            Serial.println(F("Send failed: channel busy (CD pin stayed high)")); break;
        case NRF905_SEND_TX_TIMEOUT:
            Serial.println(F("Send failed: radio never reported TX complete (see /pins)")); break;
        default:
            Serial.println(F("Send failed")); break;
    }
#else
    Serial.println(F("Send failed"));
#endif
}

/* Collect characters until Enter, without blocking the radio. */
static void pollSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (overflow)     Serial.println(F("Line too long, ignored"));
            else if (lineLen) { lineBuf[lineLen] = 0; handleLine(lineBuf); }
            lineLen  = 0;
            overflow = false;
        } else if (lineLen < LINE_MAX - 1) {
            lineBuf[lineLen++] = c;
        } else {
            overflow = true;
        }
    }
}

void setup() {
    Serial.begin(115200);
    if (!mesh.begin()) {
        Serial.println(F("Radio init failed - check wiring and power"));
        while (true);
    }
    mesh.setRxCallback(onMessage);
    Serial.print(F("Node "));
    Serial.print(NODE_ID);
    Serial.println(F(" ready"));
    printHelp();
}

void loop() {
    mesh.update();     /* receive, dedupe, relay, deliver */
    pollSerial();      /* typed messages */
}