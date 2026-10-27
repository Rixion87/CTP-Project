/*radioNRF905.h*/
/* RadioHAL driver for the nRF905 (Arduino Uno / Mega 2560), built on Zak Kemble's nRF905 library
   that is vendored next to this file.

   AIR FORMAT: the nRF905 only sends fixed 32-byte payloads, so every mesh frame is wrapped as
   [len][frame...][zero padding] (see fixedFrame.h). A mesh frame must therefore be <= 31 bytes:
   build with -DMAX_PAYLOAD_LEN=17 (10 header + 17 payload + 4 MIC).

   RECEIVING works in two modes:
     * polled (default): needs only SPI plus the CE, TXEN and PWR pins. receive() polls the radio.
     * interrupt: also wire DR (AM optional) and build with -DRADIO_PIN_DR=2 [-DRADIO_PIN_AM=3].
   Only one RadioNRF905 object can exist (the library's callbacks are plain function pointers).

   NOTE: the nRF905 is a 3.3 V part. Power it from 3.3 V and level-shift the 5 V Uno/Mega outputs. */

#pragma once
#include <Arduino.h>
#include <SPI.h>
#include "nRF905.h"
#include <radioHAL.h>
#include <fixedFrame.h>

/* ---- Pin map (override with -DRADIO_PIN_xxx=n in platformio.ini) ---------------------------- */
#ifndef RADIO_PIN_CSN
#define RADIO_PIN_CSN   10                  /* SPI chip select */
#endif
#ifndef RADIO_PIN_CE
#define RADIO_PIN_CE    7                   /* TRX_CE: standby control */
#endif
#ifndef RADIO_PIN_TXEN
#define RADIO_PIN_TXEN  8                  /* TX_EN: RX/TX select */
#endif
#ifndef RADIO_PIN_PWR
#define RADIO_PIN_PWR   9                   /* PWR_UP */
#endif
#ifndef RADIO_PIN_CD
#define RADIO_PIN_CD    NRF905_PIN_UNUSED   /* carrier detect: wire it to enable listen-before-talk */
#endif
#ifndef RADIO_PIN_DR
#define RADIO_PIN_DR    NRF905_PIN_UNUSED   /* data ready: Uno INT0 = pin 2 */
#endif
#ifndef RADIO_PIN_AM
#define RADIO_PIN_AM    NRF905_PIN_UNUSED   /* address match: Uno INT1 = pin 3 */
#endif

/* ---- Radio settings --------------------------------------------------------------------------- */
#ifndef RADIO_BAND
#define RADIO_BAND      NRF905_BAND_915     /* 868/915 MHz module (use NRF905_BAND_433 for 433 MHz modules) */
#endif
#ifndef RADIO_CHANNEL
#define RADIO_CHANNEL   383                 /* 844.8 MHz + 383 * 0.2 MHz = 921.4 MHz */
#endif
#ifndef RADIO_NET_ADDR
#define RADIO_NET_ADDR  0xA94EC554UL        /* shared by every node = the network ID. Datasheet-friendly: no repeated bytes */
#endif
#ifndef RADIO_TX_TIMEOUT_MS
#define RADIO_TX_TIMEOUT_MS 30              /* a 32-byte frame takes about 6 ms on air */
#endif
#ifndef RADIO_TX_RETRIES
#define RADIO_TX_RETRIES 4                  /* back-off attempts when carrier detect says the channel is busy */
#endif

#define NRF905_RSSI_UNKNOWN (-32768)        /* the nRF905 has no RSSI measurement */

#if RADIO_PIN_DR != NRF905_PIN_UNUSED
  #define NRF905_USE_IRQ 1
#endif

static_assert(1 + HEADER_LEN + MAX_PAYLOAD_LEN + MIC_LEN <= NRF905_MAX_PAYLOAD,
              "nRF905 frame (plus 1 length byte) must fit in 32 bytes: build with -DMAX_PAYLOAD_LEN=17");

/* Why the last send() returned what it did (printed by main.cpp when a send fails). */
enum NRF905SendStatus : uint8_t {
    NRF905_SEND_OK = 0,
    NRF905_SEND_TOO_LONG,        /* frame doesn't fit the 32-byte payload */
    NRF905_SEND_CHANNEL_BUSY,    /* carrier-detect pin stayed high through every retry */
    NRF905_SEND_TX_TIMEOUT       /* radio never reported "transmit complete" */
};

class RadioNRF905 : public RadioHAL {
public:
    /* Returns false if the radio doesn't answer over SPI (wiring / power). */
    bool begin() override {
        self() = this;
        randomSeed(analogRead(A0));          /* leave A0 unconnected: noise seeds the back-off */
        SPI.begin();

#ifdef NRF905_USE_IRQ
        _nrf.begin(SPI, 10000000UL, RADIO_PIN_CSN, RADIO_PIN_CE, RADIO_PIN_TXEN, RADIO_PIN_PWR,
                   RADIO_PIN_CD, RADIO_PIN_DR, RADIO_PIN_AM, isrDR, isrAM);
#else
        _nrf.begin(SPI, 10000000UL, RADIO_PIN_CSN, RADIO_PIN_CE, RADIO_PIN_TXEN, RADIO_PIN_PWR,
                   RADIO_PIN_CD, NRF905_PIN_UNUSED, NRF905_PIN_UNUSED, NULL, NULL);
#endif
        _nrf.events(onRxComplete, onRxInvalid, onTxComplete, NULL);
        _nrf.setBand(RADIO_BAND);
        _nrf.setChannel(RADIO_CHANNEL);
        _nrf.setListenAddress(RADIO_NET_ADDR);

        /* Read the config back: a missing or miswired radio won't return what we just wrote. */
        uint8_t regs[NRF905_REGISTER_COUNT];
        _nrf.getConfigRegisters(regs);
        bool ok = (regs[0] == (uint8_t)(RADIO_CHANNEL & 0xFF)) &&
                  ((regs[1] & 0x01) == ((RADIO_CHANNEL >> 8) & 0x01));

        _rxReady = false;
        _txDone  = false;
        _nrf.RX();                           /* start listening */
        return ok;
    }

    /* Blocks until the frame is on air (about 6 ms). Returns false if too long, channel busy, or no TX-done. */
    bool send(const uint8_t* data, uint8_t len) override {
        uint8_t raw[NRF905_MAX_PAYLOAD];
        if (!fixedFrameEncode(data, len, raw, sizeof(raw))) { _lastSend = NRF905_SEND_TOO_LONG; return false; }

        _txDone = false;
        _nrf.write(RADIO_NET_ADDR, raw, sizeof(raw));

        uint8_t tries = RADIO_TX_RETRIES;
        while (!_nrf.TX(NRF905_NEXTMODE_STANDBY, true)) {   /* false = carrier detected (needs CD wired) */
            if (--tries == 0) { _lastSend = NRF905_SEND_CHANNEL_BUSY; return false; }
            delay(random(2, 12));                           /* short random back-off */
        }

        uint32_t t0 = millis();
        while (!_txDone && (millis() - t0) < RADIO_TX_TIMEOUT_MS)
            _nrf.poll();                                    /* no-op in interrupt mode */

        bool ok = _txDone;
        _lastSend = ok ? NRF905_SEND_OK : NRF905_SEND_TX_TIMEOUT;
        _rxReady = false;                                   /* entering TX cleared any unread RX payload */
        _nrf.RX();                                          /* back to listening */
        _nrf.poll();                                        /* resync the polled-mode edge detector */
        return ok;
    }

    /* Non-blocking. Returns the mesh frame length, 0 if nothing waiting, -1 if the frame was malformed. */
    int16_t receive(uint8_t* buf, uint8_t maxLen) override {
        _nrf.poll();                                        /* no-op in interrupt mode */
        if (!_rxReady) return 0;
        _rxReady = false;

        uint8_t raw[NRF905_MAX_PAYLOAD];
        _nrf.read(raw, sizeof(raw));                        /* must drain all 32 bytes or DR never clears */
        _nrf.poll();                                        /* resync the polled-mode edge detector after the read */
        return fixedFrameDecode(raw, sizeof(raw), buf, maxLen);
    }

    int16_t getRSSI() override { return NRF905_RSSI_UNKNOWN; }
    float   getSNR()  override { return 0.0f; }

    /* The nRF905 supports -10, -2, 6 and 10 dBm. dbm is unsigned in the HAL, so -10 can't be requested. */
    void setTxPower(uint8_t dbm) override {
        if      (dbm >= 10) _nrf.setTransmitPower(NRF905_PWR_10);
        else if (dbm >= 6)  _nrf.setTransmitPower(NRF905_PWR_6);
        else                _nrf.setTransmitPower(NRF905_PWR_n2);
    }

    uint16_t invalidFrames() const { return _invalid; }    /* CRC/sync failures heard, for link testing */
    NRF905SendStatus lastSendStatus() const { return _lastSend; }

private:
    nRF905 _nrf;
    volatile bool     _rxReady = false;
    volatile bool     _txDone  = false;
    volatile uint16_t _invalid = 0;
    NRF905SendStatus  _lastSend = NRF905_SEND_OK;

    static RadioNRF905*& self() { static RadioNRF905* p = nullptr; return p; }

    static void onRxComplete(nRF905*) { self()->_rxReady = true; }
    static void onRxInvalid(nRF905*)  { self()->_invalid++; }
    static void onTxComplete(nRF905*) { self()->_txDone = true; }

#ifdef NRF905_USE_IRQ
    static void isrDR() { self()->_nrf.interrupt_dr(); }
    static void isrAM() { self()->_nrf.interrupt_am(); }
#endif
};