#include "varint.h"

uint64_t decode_varint(const uint8_t *buf, size_t buf_len, size_t offset, int *bytes_consumed) {
    if (offset >= buf_len) { *bytes_consumed = -1; return 0; }
    
    uint8_t first = buf[offset];

    if (first < 0xFD) {
        *bytes_consumed = 1;
        return first;
    } else if (first == 0xFD) {
        if (offset + 3 > buf_len) { *bytes_consumed = -1; return 0; }
        *bytes_consumed = 3;
        return (uint64_t)buf[offset+1] | ((uint64_t)buf[offset+2] << 8);
    } else if (first == 0xFE) {
        if (offset + 5 > buf_len) { *bytes_consumed = -1; return 0; }
        *bytes_consumed = 5;
        uint64_t v = 0;
        for (int i = 0; i < 4; i++) {
            v |= ((uint64_t)buf[offset+1+i] << (8 * i));
        }
        return v;
    } else { /* 0xFF */
        if (offset + 9 > buf_len) { *bytes_consumed = -1; return 0; }
        *bytes_consumed = 9;
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) {
            v |= ((uint64_t)buf[offset+1+i] << (8 * i));
        }
        return v;
    }
}