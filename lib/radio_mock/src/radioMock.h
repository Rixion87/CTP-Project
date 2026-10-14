#pragma once
/* Mock radio for native mesh protocol tests*/


#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <deque>
#include <random>
#include <mesh_platform.h>

namespace mock {

struct Frame {
    uint8_t data[255];
    uint8_t len;
    int16_t rssi;
};



class Air {
    public:
        static constexpr int MAX_NODES = 16;

        explicit Air(uint32_t seed = 1) : rng_(seed) {
            reset();
        }

        void reset() {
            for (int i = 0; i < MAX_NODES; i++) {
                inbox_[i].clear();
                txCount_[i] = rxCount_[i] = 0; 

                for (int j = 0; j < MAX_NODES; j++) {
                    linked_[i][j] = false; loss_[i]
                 }
            }   
            meshplat::nowRef() = 0;
        }

            void link(int a, int b, float loss = 0.0f, int16_6 rssi = -80) {
                linked_[a][b] = linked_[b][a] = true; 
                loss_[a][b] = loss_[b][a] = loss; 
                rssi_[a][b] = rssi[b][a] = rssi; 
            }
            void unlink(int a, int b) {
                linked_[a][b] = linked_[b][a] = false; 
            }

            void send(int from, const uint8_t* d, uint8_t len) {
                txCount_[from]++; 
                for (int to = 0; to < MAX_NODES; to++) {
                    if (to == from || !linked_[from][to])
                        continue;
                    if (dist_(rng_) < loss_[from][to])
                        continue;
                    Frame f;
                    memcpy(f.data, d, len);
                    f.len = len; 
                    f.rssi = rssi_[from][to];
                    inbox_[to].push_back(f);
                }
            }   

            bool pop(int id, Frame& out) {
                if (inbox_[id].empty())
                    return false;
                out = inbox_[id].front();
                inbox_[id].pop_front();
                rxCount_[id]++;
                return true; 
            } 

            int txCount(int id) const { 
                return txCount_[id]; 
            }

            int rxCount(int id) const { 
                return rxCount_[id]; 
            }

            int totalTx() const { 
                int s = 0; 
                for (int i = 0; i < MAX_NODES; i++) 
                    s += txCount_[i]; 
                return s; 
            }
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

inline void advanceMs(uint32_t ms) {
    meshplat::nowRef() += ms;
}


} /*Namespace Mock*/

class RadioMock {
    public:
    RadioMock(mock::Air& air, int id, size_t maxPayload = 255, bool hasRssi = true)
      : air_(air), id_(id), maxPayload_(maxPayload), hasRssi_(hasRssi) {}

  bool begin() { return true; }

  bool send(const uint8_t* data, size_t len) {
    if (len == 0 || len > maxPayload_) return false;  // catches oversize packets on the PC
    air_.transmit(id_, data, static_cast<uint8_t>(len));
    return true;
  }

  // Returns bytes copied into buf, or 0 if nothing waiting.
  int receive(uint8_t* buf, size_t cap, int16_t* rssi = nullptr) {
    mock::Frame f;
    if (!air_.pop(id_, f)) return 0;
    size_t n = f.len < cap ? f.len : cap;
    memcpy(buf, f.data, n);
    if (rssi) *rssi = hasRssi_ ? f.rssi : 0;
    return static_cast<int>(n);
  }

  size_t maxPayload() const { return maxPayload_; }
  bool hasRssi() const { return hasRssi_; }

 private:
  mock::Air& air_;
  int id_;
  size_t maxPayload_;
  bool hasRssi_;
}
