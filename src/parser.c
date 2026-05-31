#include "parser.h"
#include "varint.h"
#include <string.h>

#define BITCOIN_MAGIC 0x0709110B

static uint32_t read_u32(const uint8_t *b, size_t o) {
    return (uint32_t)b[o] |
           ((uint32_t)b[o + 1] << 8) |
           ((uint32_t)b[o + 2] << 16) |
           ((uint32_t)b[o + 3] << 24);
}

int parse_block(ParseCtx *ctx) {
    if (ctx->offset + 8 > ctx->len) return 1;
    if (ctx->depth > ctx->max_allowed_depth) return 1;

    uint32_t magic = read_u32(ctx->data, ctx->offset);
    if (magic != BITCOIN_MAGIC) return 1;
    ctx->offset += 4;

    uint32_t block_size = read_u32(ctx->data, ctx->offset);
    ctx->offset += 4;

    if (ctx->offset + block_size > ctx->len) return 1;
    size_t block_end = ctx->offset + block_size;

    if (ctx->offset + 80 > ctx->len) return 1;
    ctx->offset += 80;

    int consumed = 0;
    uint64_t tx_count = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
    if (consumed < 0) return 1;
    ctx->offset += consumed;

    ctx->stats->total_blocks++;
    ctx->stats->total_bytes_parsed += 8 + block_size;
    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;

    int r = parse_tx_list(ctx, tx_count);

    ctx->depth--;
    ctx->offset = block_end;
    return r;
}

int parse_tx_list(ParseCtx *ctx, uint64_t tx_count) {
    for (uint64_t i = 0; i < tx_count; i++) {
        if (ctx->offset >= ctx->len) return 1;
        int r = parse_tx(ctx);
        if (r != 0) return r;
    }
    return 0;
}

int parse_tx(ParseCtx *ctx) {
    if (ctx->offset + 4 > ctx->len) return 1;
    if (ctx->depth > ctx->max_allowed_depth) return 1;

    ctx->offset += 4;
    ctx->stats->total_transactions++;
    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;

    int consumed = 0;
    uint64_t in_count = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
    if (consumed < 0) {
        ctx->depth--;
        return 1;
    }
    ctx->offset += consumed;

    if (parse_inputs(ctx, in_count) != 0) {
        ctx->depth--;
        return 1;
    }

    uint64_t out_count = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
    if (consumed < 0) {
        ctx->depth--;
        return 1;
    }
    ctx->offset += consumed;

    if (parse_outputs(ctx, out_count) != 0) {
        ctx->depth--;
        return 1;
    }

    if (ctx->offset + 4 > ctx->len) {
        ctx->depth--;
        return 1;
    }
    ctx->offset += 4;

    ctx->depth--;
    return 0;
}

int parse_inputs(ParseCtx *ctx, uint64_t input_count) {
    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;
    if (ctx->depth > ctx->max_allowed_depth) {
        ctx->depth--;
        return 1;
    }

    for (uint64_t i = 0; i < input_count; i++) {
        if (ctx->offset + 36 > ctx->len) {
            ctx->depth--;
            return 1;
        }
        ctx->offset += 36;

        int consumed = 0;
        uint64_t script_len = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
        if (consumed < 0) {
            ctx->depth--;
            return 1;
        }
        ctx->offset += consumed;

        if (parse_script(ctx, script_len) != 0) {
            ctx->depth--;
            return 1;
        }

        if (ctx->offset + 4 > ctx->len) {
            ctx->depth--;
            return 1;
        }
        ctx->offset += 4;
        ctx->stats->total_inputs++;
    }

    ctx->depth--;
    return 0;
}

int parse_outputs(ParseCtx *ctx, uint64_t output_count) {
    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;
    if (ctx->depth > ctx->max_allowed_depth) {
        ctx->depth--;
        return 1;
    }

    for (uint64_t i = 0; i < output_count; i++) {
        if (ctx->offset + 8 > ctx->len) {
            ctx->depth--;
            return 1;
        }
        ctx->offset += 8;

        int consumed = 0;
        uint64_t script_len = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
        if (consumed < 0) {
            ctx->depth--;
            return 1;
        }
        ctx->offset += consumed;

        if (parse_script(ctx, script_len) != 0) {
            ctx->depth--;
            return 1;
        }

        ctx->stats->total_outputs++;
    }

    ctx->depth--;
    return 0;
}

int parse_script(ParseCtx *ctx, uint64_t script_len) {
    if (ctx->offset + script_len > ctx->len) return 1;

    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;
    if (ctx->depth > ctx->max_allowed_depth) {
        ctx->depth--;
        return 1;
    }

    ctx->offset += script_len;
    ctx->stats->total_scripts++;
    ctx->stats->total_chunks++;

    ctx->depth--;
    return 0;
}
