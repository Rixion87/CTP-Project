#include <unity.h>
#include <radioMock.h>

void setUp(void) {}
void tearDown(void) {}

void test_delivers_to_linked_neighbour_only() {
    mock::Air air;
    RadioMock a(air, 0), b(air, 1), c(air, 2);
    air.link(0, 1);                       // A-B linked, C isolated

    uint8_t msg[] = {1, 2, 3};
    TEST_ASSERT_TRUE(a.send(msg, sizeof(msg)));

    uint8_t buf[32];
    TEST_ASSERT_EQUAL(3, b.receive(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msg, buf, 3);
    TEST_ASSERT_EQUAL(0, c.receive(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(0, a.receive(buf, sizeof(buf)));   // sender doesn't hear itself
}

void test_respects_max_payload() {
    mock::Air air;
    RadioMock nrf(air, 0, 32);            // nRF905-like
    uint8_t big[33] = {0}, ok[32] = {0};
    TEST_ASSERT_FALSE(nrf.send(big, sizeof(big)));
    TEST_ASSERT_TRUE(nrf.send(ok, sizeof(ok)));
}

void test_total_loss_and_no_loss() {
    mock::Air air(42);
    RadioMock a(air, 0), b(air, 1);
    uint8_t msg[] = {9}, buf[8];

    air.link(0, 1, 1.0f);                 // always dropped
    a.send(msg, 1);
    TEST_ASSERT_EQUAL(0, b.receive(buf, sizeof(buf)));

    air.link(0, 1, 0.0f);                 // never dropped
    a.send(msg, 1);
    TEST_ASSERT_EQUAL(1, b.receive(buf, sizeof(buf)));
}

static int lossyRun(uint32_t seed) {
    mock::Air air(seed);
    RadioMock a(air, 0), b(air, 1);
    air.link(0, 1, 0.5f);
    uint8_t m[] = {1}, buf[4];
    int got = 0;
    for (int i = 0; i < 100; i++) { a.send(m, 1); got += b.receive(buf, sizeof(buf)) ? 1 : 0; }
    return got;
}
void test_loss_is_repeatable_for_same_seed() { TEST_ASSERT_EQUAL(lossyRun(7), lossyRun(7)); }

void test_rssi_follows_link() {
    mock::Air air;
    RadioMock a(air, 0), b(air, 1);
    air.link(0, 1, 0.0f, -95);
    uint8_t m[] = {1}, buf[4];
    a.send(m, 1);
    b.receive(buf, sizeof(buf));
    TEST_ASSERT_EQUAL(-95, b.getRSSI());
}

void test_virtual_clock() {
    mock::Air air;                        // reset() zeroes the clock
    TEST_ASSERT_EQUAL_UINT32(0, meshMillis());
    mock::advanceMs(250);
    TEST_ASSERT_EQUAL_UINT32(250, meshMillis());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_delivers_to_linked_neighbour_only);
    RUN_TEST(test_respects_max_payload);
    RUN_TEST(test_total_loss_and_no_loss);
    RUN_TEST(test_loss_is_repeatable_for_same_seed);
    RUN_TEST(test_rssi_follows_link);
    RUN_TEST(test_virtual_clock);
    return UNITY_END();
}
