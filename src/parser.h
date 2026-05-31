#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stddef.h>

/* Aggregated statistics collected during recursive traversal */
typedef struct {
    int max_depth;              /* deepest recursion level reached */
    long total_chunks;          /* total structural units visited */
    long total_blocks;
    long total_transactions;
    long total_inputs;
    long total_outputs;
    long total_scripts;
    long total_bytes_parsed;
    double avg_tx_size;         /* computed post-traversal */
    double avg_script_size;
    double elapsed_ms;          /* wall-clock time for the parse */
} Stats;

/* Context passed down through every recursive call */
typedef struct {
    const uint8_t *data;        /* pointer to full file buffer */
    size_t len;                 /* total length of buffer */
    size_t offset;              /* current read position */
    int depth;                  /* current recursion depth */
    int max_allowed_depth;      /* stack overflow guard */
    Stats *stats;
} ParseCtx;

/* Function declarations */
int parse_block(ParseCtx *ctx);
int parse_tx_list(ParseCtx *ctx, uint64_t tx_count);
int parse_tx(ParseCtx *ctx);
int parse_inputs(ParseCtx *ctx, uint64_t input_count);
int parse_outputs(ParseCtx *ctx, uint64_t output_count);
int parse_script(ParseCtx *ctx, uint64_t script_len);

#endif