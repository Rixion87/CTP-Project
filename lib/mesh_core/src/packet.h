/*packet.h*/
/*Responsible for header struct, packet types, serialisation, CRC/MAC fields and payload size limit*/

#pragma once
#include <stdint.h>
#include <stddef.h>

#define PROTO_VERSION       1
#ifndef MAX_PAYLOAD_LEN
#define MAX_PAYLOAD_LEN     200   /* nRF905 builds: -DMAX_PAYLOAD_LEN=18 so the whole frame fits in 32 bytes */
#endif
#define HEADER_LEN          10 /*bytes before payload*/
#define MIC_LEN             4 /*truncated HMAC at tail*/

/* Packet types*/
enum PacketType : uint8_t {
    PKT_MESSAGE = 0x01, /*USER PAYLOAD*/
    PKT_BEACON  = 0x02, /*NEIGHBOUR DISCOVERY*/
    PKT_ACK     = 0x03, /*DELIVERY ACKNOLEDGEMENT */
    PKT_ROUTE   = 0x04  /*ROUTING INFO*/
};

/* In-memory packet. NOT the wire format: the MIC sits after the full payload array here,
   but on the wire it follows the *actual* payload. mesh.h serialises/parses. */
struct __attribute__((packed)) MeshPacket {
    uint8_t     version;        /* PROTOCOL VERSION*/
    uint8_t     type;           /*PACKET TYPE*/
    uint16_t    srcID;          /*SOURCE NODE*/
    uint16_t    destID;         /*DESTINATION NODE*/
    uint8_t     ttl;            /*DECREMENTS EACH HOP, PACKET DROPPED AT 0*/
    uint8_t     seqNum;         /*PER-SOURCE SEQUENCE NUMBER*/
    uint16_t    payloadLen;     /* BYTES OF PAYLOAD FOLLOWING*/
    uint8_t     payload[MAX_PAYLOAD_LEN];
    uint8_t     mic[MIC_LEN];   /*MESSAGE INTEGRITY CODE (TAIL)*/
};

static_assert(offsetof(MeshPacket, payload) == HEADER_LEN, "HEADER_LEN out of sync with MeshPacket");
static_assert(HEADER_LEN + MAX_PAYLOAD_LEN + MIC_LEN <= 255, "wire frame must fit a uint8_t length");

#define BROADCAST_ADDR      0xFFFF
#define MAX_TTL             7
