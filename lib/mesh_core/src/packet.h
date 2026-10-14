/*packet.h*/
/*Responsible for header struct, packet types, serialisation, CRC/MAC fields and payload size limit*/

#pragma once
#include <stdint.h>

#define PROTO_VERSION       1
#define MAX_PAYLOAD_LEN     200
#define HEADER_LEN          10 /*bytes before payload*/
#define MIC_LEN             4 /*truncated HMAC at tail*/

/* Packet types*/
enum PacketType : uint8_t {
    PKT_MESSAGE = 0x01, /*USER PAYLOAD*/
    PKT_BEACON  = 0x02, /*NEIGHBOUR DISCOVERY*/
    PKT_ACK     = 0x03, /*DELIVERY ACKNOLEDGEMENT */
    PKT_ROUTE   = 0x04  /*ROUTING INFO*/
};

/* Wire Format*/
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

#define BROADCAST_ADDR      0xFFFF
#define MAX_TTL             7