#ifndef VARINT_H
#define VARINT_H

#include <stdint.h>
#include <stddef.h>

uint64_t decode_varint(const uint8_t *buf, size_t buf_len, size_t offset, int *bytes_consumed);

#endif