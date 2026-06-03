#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "parser.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: rbda <path-to-blk.dat>\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    uint8_t *buf = malloc(fsize);
    if (!buf) { fprintf(stderr, "Out of memory\n"); return 1; }
    
    fread(buf, 1, fsize, f);
    fclose(f);


    Stats stats = {0};
    ParseCtx ctx = {
        .data = buf,
        .len = fsize,
        .offset = 0,
        .depth = 0,
        .max_allowed_depth = 64,
        .stats = &stats
    };

    clock_t t0 = clock();

    /* Bulletproof Gap-Skip Loop */
    while (ctx.offset + 4 <= ctx.len) {
        uint32_t magic = (uint32_t)ctx.data[ctx.offset] | 
                         ((uint32_t)ctx.data[ctx.offset+1] << 8) | 
                         ((uint32_t)ctx.data[ctx.offset+2] << 16) | 
                         ((uint32_t)ctx.data[ctx.offset+3] << 24);
                         
        /* Support Mainnet, Testnet3, Testnet4, Regtest, and Signet */
        if (magic == 0xD9B4BEF9 || magic == 0x0709110B || magic == 0x1C163F28 || magic == 0xDAB5BFFA || magic == 0x40CF030A) {
            size_t saved_offset = ctx.offset;
            if (parse_block(&ctx) != 0) {
                /* False positive or cut-off block at EOF. Skip 1 byte and keep scanning. */
                ctx.offset = saved_offset + 1;
                continue;
            }
            continue; /* parse_block succeeded and advanced the offset for us */
        }
        ctx.offset++;
    }

    clock_t t1 = clock();
    stats.elapsed_ms = 1000.0 * (t1 - t0) / CLOCKS_PER_SEC;

    /* Print stats as key=value for dashboard to parse */
    printf("max_depth=%d\n", stats.max_depth);
    printf("total_blocks=%ld\n", stats.total_blocks);
    printf("total_transactions=%ld\n", stats.total_transactions);
    printf("total_inputs=%ld\n", stats.total_inputs);
    printf("total_outputs=%ld\n", stats.total_outputs);
    printf("total_scripts=%ld\n", stats.total_scripts);
    printf("total_bytes=%ld\n", stats.total_bytes_parsed);
    printf("elapsed_ms=%.2f\n", stats.elapsed_ms);

    free(buf);
    return 0;
}