#include <unity.h>
#include <string.h>
#include <fixedFrame.h>

void setUp(void) {}
void tearDown(void) {}

void test_roundtrip() {
    uint8_t frame[] = {1, 2, 3, 4, 5}, raw[32], out[32];
    TEST_ASSERT_TRUE(fixedFrameEncode(frame, sizeof(frame), raw, sizeof(raw)));
    TEST_ASSERT_EQUAL(5, raw[0]);                                   // length prefix
    TEST_ASSERT_EQUAL(5, fixedFrameDecode(raw, sizeof(raw), out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame, out, 5);
}

void test_padding_is_zero_even_over_stale_data() {
    uint8_t frame[] = {9, 9, 9}, raw[32];
    memset(raw, 0xAA, sizeof(raw));
    fixedFrameEncode(frame, sizeof(frame), raw, sizeof(raw));
    for (int i = 4; i < 32; i++) TEST_ASSERT_EQUAL_UINT8(0, raw[i]);
}

void test_largest_frame_is_fixedSize_minus_one() {
    uint8_t frame[32], raw[32], out[32];
    for (int i = 0; i < 32; i++) frame[i] = (uint8_t)(i + 1);
    TEST_ASSERT_TRUE (fixedFrameEncode(frame, 31, raw, 32));
    TEST_ASSERT_EQUAL(31, fixedFrameDecode(raw, 32, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame, out, 31);
    TEST_ASSERT_FALSE(fixedFrameEncode(frame, 32, raw, 32));        // no room for the length byte
}

void test_encode_rejects_empty_frame() {
    uint8_t raw[32], frame[1] = {0};
    TEST_ASSERT_FALSE(fixedFrameEncode(frame, 0, raw, 32));
}

void test_decode_rejects_bad_length_byte() {
    uint8_t raw[32] = {0}, out[32];
    raw[0] = 0;   TEST_ASSERT_EQUAL(-1, fixedFrameDecode(raw, 32, out, sizeof(out)));   // empty / blank air
    raw[0] = 32;  TEST_ASSERT_EQUAL(-1, fixedFrameDecode(raw, 32, out, sizeof(out)));   // claims more than fits
    raw[0] = 255; TEST_ASSERT_EQUAL(-1, fixedFrameDecode(raw, 32, out, sizeof(out)));
}

void test_decode_rejects_frame_larger_than_caller_buffer() {
    uint8_t frame[20] = {0}, raw[32], out[10];
    fixedFrameEncode(frame, sizeof(frame), raw, 32);
    TEST_ASSERT_EQUAL(-1, fixedFrameDecode(raw, 32, out, sizeof(out)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_padding_is_zero_even_over_stale_data);
    RUN_TEST(test_largest_frame_is_fixedSize_minus_one);
    RUN_TEST(test_encode_rejects_empty_frame);
    RUN_TEST(test_decode_rejects_bad_length_byte);
    RUN_TEST(test_decode_rejects_frame_larger_than_caller_buffer);
    return UNITY_END();
}
