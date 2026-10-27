/*fixedFrame.h*/
/* Length-prefixed framing for radios that only send a fixed-size payload (e.g. nRF905: always 32 bytes).
   On-air layout:  [len][frame bytes ...][zero padding up to fixedSize]
   One byte is used for the length, so the largest frame is fixedSize - 1. */

#pragma once
#include <stdint.h>
#include <string.h>

/* Build the fixed-size on-air payload. Returns false if the frame is empty or too long. */
inline bool fixedFrameEncode(const uint8_t* frame, uint8_t len, uint8_t* out, uint8_t fixedSize) {
    if (len == 0 || len > fixedSize - 1) return false;
    out[0] = len;
    memcpy(out + 1, frame, len);
    memset(out + 1 + len, 0, fixedSize - 1 - len);
    return true;
}

/* Recover the frame. Returns its length, or -1 if the payload is malformed or doesn't fit in out. */
inline int16_t fixedFrameDecode(const uint8_t* in, uint8_t fixedSize, uint8_t* out, uint8_t maxLen) {
    uint8_t len = in[0];
    if (len == 0 || len > fixedSize - 1 || len > maxLen) return -1;
    memcpy(out, in + 1, len);
    return len;
}
