#include <unity.h>
#include <string.h>
#include <radioMock.h>
#include <mesh.h>

struct Inbox { int count; uint16_t lastSrc; uint16_t lastLen; char lastText[MAX_PAYLOAD_LEN + 1]; };

static void onRx(const MeshPacket& p, void* ctx) {
    Inbox* in = static_cast<Inbox*>(ctx);
    in->count++;
    in->lastSrc = p.srcID;
    in->lastLen = p.payloadLen;
    memcpy(in->lastText, p.payload, p.payloadLen);
    in->lastText[p.payloadLen] = 0;
}

// N nodes sharing one simulated air. Node i has mesh ID i+1 and radio/air id i.
struct Sim {
    static constexpr int MAX = mock::Air::MAX_NODES;
    mock::Air  air;
    RadioMock* radio[MAX] = {};
    MeshNode*  node[MAX]  = {};
    Inbox      inbox[MAX] = {};
    int n;

    explicit Sim(int count, uint8_t maxPayload = 255, uint32_t seed = 1) : air(seed), n(count) {
        for (int i = 0; i < n; i++) {
            radio[i] = new RadioMock(air, i, maxPayload);
            node[i]  = new MeshNode(radio[i], (uint16_t)(i + 1));
            node[i]->setRxCallback(onRx, &inbox[i]);
            node[i]->begin();
        }
    }
    ~Sim() { for (int i = 0; i < n; i++) { delete node[i]; delete radio[i]; } }

    void chain() { for (int i = 0; i + 1 < n; i++) air.link(i, i + 1); }

    void run(uint32_t ms) {
        for (uint32_t t = 0; t < ms; t++) {
            for (int i = 0; i < n; i++) node[i]->update();
            mock::advanceMs(1);
        }
    }
};

void setUp(void) {}
void tearDown(void) {}

// ---- delivery & relaying ----------------------------------------------------

void test_two_hop_delivery_and_payload_integrity() {
    Sim s(3); s.chain();                                   // A - B - C
    TEST_ASSERT_TRUE(s.node[0]->sendMessage(3, "hello"));
    s.run(100);
    TEST_ASSERT_EQUAL(1, s.inbox[2].count);
    TEST_ASSERT_EQUAL(0, s.inbox[1].count);                // B only relays
    TEST_ASSERT_EQUAL_STRING("hello", s.inbox[2].lastText);
    TEST_ASSERT_EQUAL(5, s.inbox[2].lastLen);
    TEST_ASSERT_EQUAL(1, s.inbox[2].lastSrc);
}

void test_broadcast_reaches_everyone_once() {
    Sim s(3); s.chain();
    s.node[0]->sendMessage(BROADCAST_ADDR, "hi all");
    s.run(100);
    TEST_ASSERT_EQUAL(1, s.inbox[1].count);
    TEST_ASSERT_EQUAL(1, s.inbox[2].count);
    TEST_ASSERT_EQUAL(0, s.inbox[0].count);                // hears relays of its own packet, drops them
}

void test_duplicate_suppression_in_diamond() {
    Sim s(4);                                              // A-B-D and A-C-D
    s.air.link(0, 1); s.air.link(0, 2); s.air.link(1, 3); s.air.link(2, 3);
    s.node[0]->sendMessage(4, "x");
    s.run(100);
    TEST_ASSERT_EQUAL(1, s.inbox[3].count);                // two copies arrive, one delivered
}

void test_flood_terminates_with_bounded_transmissions() {
    Sim s(5);
    for (int i = 0; i < 5; i++) for (int j = i + 1; j < 5; j++) s.air.link(i, j);
    s.node[0]->sendMessage(5, "x");
    s.run(500);
    TEST_ASSERT_EQUAL(1, s.inbox[4].count);
    TEST_ASSERT_EQUAL(4, s.air.totalTx());                 // origin + 3 relays; destination doesn't relay
}

// ---- TTL ----------------------------------------------------------------------

static int chainDelivered(int hops) {
    Sim s(hops + 1); s.chain();
    s.node[0]->sendMessage((uint16_t)(hops + 1), "x");
    s.run(200);
    return s.inbox[hops].count;
}
void test_ttl_reaches_max_ttl_hops()      { TEST_ASSERT_EQUAL(1, chainDelivered(MAX_TTL)); }
void test_ttl_stops_beyond_max_ttl()      { TEST_ASSERT_EQUAL(0, chainDelivered(MAX_TTL + 1)); }

// ---- lossy links ----------------------------------------------------------------

void test_lossy_two_hop_statistics() {
    Sim s(3, 255, 123);
    s.air.link(0, 1, 0.3f); s.air.link(1, 2, 0.3f);
    for (int i = 0; i < 200; i++) { s.node[0]->sendMessage(3, "x"); s.run(5); }
    int got = s.inbox[2].count;                            // expect about 200 * 0.7 * 0.7 = 98
    TEST_ASSERT_GREATER_THAN(60, got);
    TEST_ASSERT_LESS_THAN(140, got);
}

// ---- frame size / validation -------------------------------------------------------

void test_nrf905_frame_limit() {
    Sim s(2, 31); s.air.link(0, 1);                        // 32-byte nRF905 payload minus 1 length byte
    TEST_ASSERT_TRUE (s.node[0]->sendMessage(2, "01234567890123456"));     // 10+17+4 = 31 bytes
    TEST_ASSERT_FALSE(s.node[0]->sendMessage(2, "012345678901234567"));    // 32 bytes, refused
    s.run(50);
    TEST_ASSERT_EQUAL(1, s.inbox[1].count);
}

void test_rejects_overlong_message() {
    Sim s(2); s.air.link(0, 1);
    char big[MAX_PAYLOAD_LEN + 2];
    memset(big, 'a', sizeof(big)); big[sizeof(big) - 1] = 0;
    TEST_ASSERT_FALSE(s.node[0]->sendMessage(2, big));
}

void test_rejects_malformed_frames() {
    Sim s(2); s.air.link(0, 1);
    // header says payloadLen = 200 but the frame is only 14 bytes long
    uint8_t bad[14] = {PROTO_VERSION, PKT_MESSAGE, 0x01, 0x00, 0x02, 0x00, MAX_TTL, 0, 200, 0, 0, 0, 0, 0};
    s.radio[0]->send(bad, sizeof(bad));
    s.run(10);
    TEST_ASSERT_EQUAL(0, s.inbox[1].count);

    bad[8] = 0;                                            // control: consistent length IS delivered
    s.radio[0]->send(bad, sizeof(bad));
    s.run(10);
    TEST_ASSERT_EQUAL(1, s.inbox[1].count);
}

// ---- SeenCache & RoutingTable -------------------------------------------------------

void test_seen_cache_starts_empty() {
    SeenCache c;
    TEST_ASSERT_FALSE(c.hasSeen(0, 0));
    TEST_ASSERT_FALSE(c.hasSeen(1, 0));
}

void test_seen_cache_evicts_oldest() {
    SeenCache c;
    for (int i = 0; i < SEEN_CACHE_SIZE; i++) c.markSeen(1, (uint8_t)i);
    TEST_ASSERT_TRUE(c.hasSeen(1, 0));
    c.markSeen(1, (uint8_t)SEEN_CACHE_SIZE);
    TEST_ASSERT_FALSE(c.hasSeen(1, 0));
    TEST_ASSERT_TRUE(c.hasSeen(1, (uint8_t)SEEN_CACHE_SIZE));
}

void test_routing_expire_removes_every_stale_entry() {
    meshplat::nowRef() = 0;
    RoutingTable r;
    r.update(1, 1, -70); r.update(2, 1, -70); r.update(3, 1, -70);
    mock::advanceMs(70000);
    r.expire();
    TEST_ASSERT_EQUAL(0, r.count());                       // would leave entries behind with the old loop
}

void test_routing_expire_keeps_fresh_entries() {
    meshplat::nowRef() = 0;
    RoutingTable r;
    r.update(1, 1, -70);
    mock::advanceMs(50000);
    r.update(2, 1, -70);
    mock::advanceMs(20000);
    r.expire();
    TEST_ASSERT_FALSE(r.knows(1));
    TEST_ASSERT_TRUE(r.knows(2));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_two_hop_delivery_and_payload_integrity);
    RUN_TEST(test_broadcast_reaches_everyone_once);
    RUN_TEST(test_duplicate_suppression_in_diamond);
    RUN_TEST(test_flood_terminates_with_bounded_transmissions);
    RUN_TEST(test_ttl_reaches_max_ttl_hops);
    RUN_TEST(test_ttl_stops_beyond_max_ttl);
    RUN_TEST(test_lossy_two_hop_statistics);
    RUN_TEST(test_nrf905_frame_limit);
    RUN_TEST(test_rejects_overlong_message);
    RUN_TEST(test_rejects_malformed_frames);
    RUN_TEST(test_seen_cache_starts_empty);
    RUN_TEST(test_seen_cache_evicts_oldest);
    RUN_TEST(test_routing_expire_removes_every_stale_entry);
    RUN_TEST(test_routing_expire_keeps_fresh_entries);
    return UNITY_END();
}
