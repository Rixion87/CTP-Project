#pragma once
// Mock radio for native (PC) tests. Implements the same RadioHAL the real drivers do.
// NATIVE ONLY: uses the C++ standard library (deque/random), which AVR/SAMD builds don't have.
#ifdef ARDUINO
  #error "radioMock.h is for native PC tests only"
#endif

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <deque>
#include <random>
#include <mesh_platform.h>
#include <radioHAL.h>

namespace mock {

struct Frame {
    uint8_t data[255];
    uint8_t len;
    int16_t rssi;
};

// Shared medium. Radios attach by integer id (0..MAX_NODES-1); links are explicit,
// so any topology (line, ring, diamond...) can be built.
class Air {
public:
    static constexpr int MAX_NODES = 16;

    explicit Air(uint32_t seed = 1) : rng_(seed) { reset(); }

    void reset() {
        for (int i = 0; i < MAX_NODES; i++) {
            inbox_[i].clear();
            txCount_[i] = rxCount_[i] = 0;
            for (int j = 0; j < MAX_NODES; j++) {
                linked_[i][j] = false; loss_[i][j] = 0.0f; rssi_[i][j] = -80;
            }
        }
        meshplat::nowRef() = 0;   // virtual clock back to zero
    }

    // Bidirectional link. loss = probability a frame is dropped (0..1).
    void link(int a, int b, float loss = 0.0f, int16_t rssi = -80) {
        linked_[a][b] = linked_[b][a] = true;
        loss_[a][b] = loss_[b][a] = loss;
        rssi_[a][b] = rssi_[b][a] = rssi;
    }
    void unlink(int a, int b) { linked_[a][b] = linked_[b][a] = false; }

    void transmit(int from, const uint8_t* d, uint8_t len) {
        txCount_[from]++;
        for (int to = 0; to < MAX_NODES; to++) {
            if (to == from || !linked_[from][to]) continue;
            if (dist_(rng_) < loss_[from][to]) continue;   // dropped on this link
            Frame f;
            memcpy(f.data, d, len);
            f.len = len;
            f.rssi = rssi_[from][to];
            inbox_[to].push_back(f);
        }
    }

    bool pop(int id, Frame& out) {
        if (inbox_[id].empty()) return false;
        out = inbox_[id].front();
        inbox_[id].pop_front();
        rxCount_[id]++;
        return true;
    }

    int txCount(int id) const { return txCount_[id]; }
    int rxCount(int id) const { return rxCount_[id]; }
    int totalTx() const { int s = 0; for (int i = 0; i < MAX_NODES; i++) s += txCount_[i]; return s; }

private:
    std::deque<Frame> inbox_[MAX_NODES];
    bool linked_[MAX_NODES][MAX_NODES];
    float loss_[MAX_NODES][MAX_NODES];
    int16_t rssi_[MAX_NODES][MAX_NODES];
    int txCount_[MAX_NODES];
    int rxCount_[MAX_NODES];
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_{0.0f, 1.0f};
};

inline void advanceMs(uint32_t ms) { meshplat::nowRef() += ms; }

} // namespace mock

class RadioMock : public RadioHAL {
public:
    // maxPayload = 32 emulates the nRF905 frame limit; 255 emulates LoRa.
    RadioMock(mock::Air& air, int id, uint8_t maxPayload = 255)
        : air_(air), id_(id), maxPayload_(maxPayload) {}

    bool begin() override { return true; }

    bool send(const uint8_t* data, uint8_t len) override {
        if (len == 0 || len > maxPayload_) return false;   // catches oversize frames on the PC
        air_.transmit(id_, data, len);
        return true;
    }

    // Returns frame length, 0 if nothing waiting, -1 if it doesn't fit buf.
    int16_t receive(uint8_t* buf, uint8_t maxLen) override {
        mock::Frame f;
        if (!air_.pop(id_, f)) return 0;
        if (f.len > maxLen) return -1;
        memcpy(buf, f.data, f.len);
        lastRssi_ = f.rssi;
        return f.len;
    }

    int16_t getRSSI() override { return lastRssi_; }
    float   getSNR() override { return 9.0f; }
    void    setTxPower(uint8_t) override {}

private:
    mock::Air& air_;
    int id_;
    uint8_t maxPayload_;
    int16_t lastRssi_ = 0;
};
