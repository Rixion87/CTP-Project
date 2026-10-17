/*mesh.h*/
/* Main header file, includes everything in mesh_core. Requiring only one include statement for radio drivers*/


#pragma once
#include <stdio.h>
#include <string.h>
#include "mesh_platform.h"
#include "radioHAL.h"
#include "packet.h"
#include "routing.h"
#include "SeenCache.h"

/* Plain function pointer (no std::function) so this works on AVR. */
typedef void (*MeshRxCallback)(const MeshPacket& pkt, void* ctx);

class MeshNode {
    RadioHAL*       _radio;
    RoutingTable    _routes;
    SeenCache       _seen;
    uint16_t        _nodeID;
    uint8_t         _seqNum = 0;
    MeshRxCallback  _rxCb  = nullptr;
    void*           _rxCtx = nullptr;

public:
    MeshNode(RadioHAL* radio, uint16_t nodeID)
        : _radio(radio), _nodeID(nodeID) {}

    void begin() { _radio->begin(); }

    /* Optional: get delivered packets instead of the default log line (used by tests). */
    void setRxCallback(MeshRxCallback cb, void* ctx = nullptr) { _rxCb = cb; _rxCtx = ctx; }

    const RoutingTable& routes() const { return _routes; }

    /*Send a packet originating from this node. Returns false if too long or the radio refused it.*/
    bool sendMessage(uint16_t destID, const char* text) {
        size_t n = strlen(text);
        if (n > MAX_PAYLOAD_LEN) return false;

        MeshPacket pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.version     = PROTO_VERSION;
        pkt.type        = PKT_MESSAGE;
        pkt.srcID       = _nodeID;
        pkt.destID      = destID;
        pkt.ttl         = MAX_TTL;
        pkt.seqNum      = _seqNum++;
        pkt.payloadLen  = (uint16_t)n;
        memcpy(pkt.payload, text, n);

        _seen.markSeen(_nodeID, pkt.seqNum); /*prevents relaying our own packets when we hear them back*/
        return transmit(pkt);
    }

    void update() {
        uint8_t buf[HEADER_LEN + MAX_PAYLOAD_LEN + MIC_LEN];
        int16_t len = _radio->receive(buf, sizeof(buf));
        if (len < HEADER_LEN + MIC_LEN)
            return;

        /*Parse the wire frame: header | payload | MIC*/
        MeshPacket pkt;
        memcpy(&pkt, buf, HEADER_LEN);
        if (pkt.version != PROTO_VERSION)
            return;
        if (pkt.payloadLen > MAX_PAYLOAD_LEN || len != HEADER_LEN + pkt.payloadLen + MIC_LEN)
            return;                                   /*malformed / truncated*/
        memcpy(pkt.payload, buf + HEADER_LEN, pkt.payloadLen);
        memcpy(pkt.mic, buf + HEADER_LEN + pkt.payloadLen, MIC_LEN);

        /*Drop any duplicate packets*/
        if (_seen.hasSeen(pkt.srcID, pkt.seqNum))
            return;
        _seen.markSeen(pkt.srcID, pkt.seqNum);

        /*Update routing table*/
        _routes.update(pkt.srcID, MAX_TTL - pkt.ttl, _radio->getRSSI());

        /*Check if packet is addressed to us*/
        if (pkt.destID == _nodeID || pkt.destID == BROADCAST_ADDR) {
            onReceive(pkt);
        }

        /*Relay if TTL allows*/
        if (pkt.ttl > 1 && pkt.destID != _nodeID) {
            pkt.ttl--;
            transmit(pkt);
        }
    }

    private:
    bool transmit(const MeshPacket& pkt) {
        uint8_t wire[HEADER_LEN + MAX_PAYLOAD_LEN + MIC_LEN];
        memcpy(wire, &pkt, HEADER_LEN);
        memcpy(wire + HEADER_LEN, pkt.payload, pkt.payloadLen);
        memcpy(wire + HEADER_LEN + pkt.payloadLen, pkt.mic, MIC_LEN);
        return _radio->send(wire, (uint8_t)(HEADER_LEN + pkt.payloadLen + MIC_LEN));
    }

    void onReceive(const MeshPacket& pkt) {
        if (_rxCb) { _rxCb(pkt, _rxCtx); return; }
        char buf[64];
        snprintf(buf, sizeof(buf),
            "[MSG] From 0x%04X -> 0x%04X: %.*s",
            (unsigned)pkt.srcID, (unsigned)pkt.destID,
            (int)pkt.payloadLen, (const char*)pkt.payload);
        meshLog(buf);
    }
};