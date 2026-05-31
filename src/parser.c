#include <stdio.h>
#include "parser.h"
#include "varint.h"

static uint32_t read_u32(const uint8_t *b, size_t o) {
    return (uint32_t)b[o] | ((uint32_t)b[o+1] << 8) | ((uint32_t)b[o+2] << 16) | ((uint32_t)b[o+3] << 24);
}

static int is_valid_magic(uint32_t m) {
    return m == 0xD9B4BEF9 || /* Mainnet */
           m == 0x0709110B || /* Testnet3 */
           m == 0x1C163F28 || /* Testnet4 (Bitcoin Core 28+) */
           m == 0xDAB5BFFA || /* Regtest */
           m == 0x40CF030A;   /* Signet */
}

int parse_block(ParseCtx *ctx) {
    if (ctx->offset + 8 > ctx->len) return 1;
    if (ctx->depth > ctx->max_allowed_depth) return 1;

    uint32_t magic = read_u32(ctx->data, ctx->offset);
    if (!is_valid_magic(magic)) return 1;
    ctx->offset += 4;

    uint32_t block_size = read_u32(ctx->data, ctx->offset);
    ctx->offset += 4;

    if (ctx->offset + block_size > ctx->len) return 1;
    size_t block_end = ctx->offset + block_size;

    ctx->offset += 80; /* skip 80-byte header */

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
    ctx->offset = block_end; /* Advance to end of block regardless of partial errors */
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

    ctx->offset += 4; /* skip version */
    ctx->stats->total_transactions++;
    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;

    /* SegWit detection */
    int has_witness = 0;
    if (ctx->offset + 2 <= ctx->len) {
        if (ctx->data[ctx->offset] == 0x00 && ctx->data[ctx->offset+1] == 0x01) {
            has_witness = 1;
            ctx->offset += 2; /* skip marker and flag */
        }
    }

    int consumed = 0;
    uint64_t in_count = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
    if (consumed < 0) { ctx->depth--; return 1; }
    ctx->offset += consumed;

    if (parse_inputs(ctx, in_count) != 0) { ctx->depth--; return 1; }

    uint64_t out_count = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
    if (consumed < 0) { ctx->depth--; return 1; }
    ctx->offset += consumed;

    if (parse_outputs(ctx, out_count) != 0) { ctx->depth--; return 1; }

    /* SegWit skip loop */
    if (has_witness) {
        for (uint64_t i = 0; i < in_count; i++) {
            uint64_t witness_count = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
            if (consumed < 0) { ctx->depth--; return 1; }
            ctx->offset += consumed;
            for (uint64_t j = 0; j < witness_count; j++) {
                uint64_t item_len = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
                if (consumed < 0) { ctx->depth--; return 1; }
                ctx->offset += consumed + item_len;
            }
        }
    }

    ctx->offset += 4; /* skip locktime */
    ctx->depth--;
    return 0;
}

int parse_inputs(ParseCtx *ctx, uint64_t input_count) {
    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;

    for (uint64_t i = 0; i < input_count; i++) {
        if (ctx->offset + 36 > ctx->len) { ctx->depth--; return 1; }
        ctx->offset += 36; /* prev hash + index */

        int consumed = 0;
        uint64_t script_len = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
        if (consumed < 0) { ctx->depth--; return 1; }
        ctx->offset += consumed;

        if (parse_script(ctx, script_len) != 0) { ctx->depth--; return 1; }
        
        ctx->offset += 4; /* skip sequence */
        ctx->stats->total_inputs++;
    }
    ctx->depth--;
    return 0;
}

int parse_outputs(ParseCtx *ctx, uint64_t output_count) {
    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;

    for (uint64_t i = 0; i < output_count; i++) {
        if (ctx->offset + 8 > ctx->len) { ctx->depth--; return 1; }
        ctx->offset += 8; /* value */

        int consumed = 0;
        uint64_t script_len = decode_varint(ctx->data, ctx->len, ctx->offset, &consumed);
        if (consumed < 0) { ctx->depth--; return 1; }
        ctx->offset += consumed;

        if (parse_script(ctx, script_len) != 0) { ctx->depth--; return 1; }
        
        ctx->stats->total_outputs++;
    }
    ctx->depth--;
    return 0;
}

int parse_script(ParseCtx *ctx, uint64_t script_len) {
    if (ctx->offset + script_len > ctx->len) return 1;
    ctx->depth++;
    if (ctx->depth > ctx->stats->max_depth) ctx->stats->max_depth = ctx->depth;

    ctx->offset += script_len;
    ctx->stats->total_scripts++;
    ctx->stats->total_chunks++;
    ctx->depth--;
    return 0;
}