#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: scanner <path-to-blk.dat>\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    uint8_t *buf = malloc(fsize);
    if (!buf) { printf("Out of memory\n"); return 1; }
    fread(buf, 1, fsize, f);
    fclose(f);

    /* 1. The XOR De-Obfuscation Step */
    uint8_t xor_key[8] = {0x86, 0xED, 0xB9, 0xE0, 0x9F, 0x3C, 0xFB, 0xF3};
    for (long i = 0; i < fsize; i++) {
        buf[i] ^= xor_key[i % 8];
    }

    /* 2. The Magic Number Scanner */
    int match_count = 0;
    printf("Scanning %ld bytes for Testnet Magic Numbers...\n", fsize);
    printf("------------------------------------------------\n");

    for (long i = 0; i <= fsize - 4; i++) {
        uint32_t magic = (uint32_t)buf[i] | 
                        ((uint32_t)buf[i+1] << 8) |
                        ((uint32_t)buf[i+2] << 16) | 
                        ((uint32_t)buf[i+3] << 24);
                        
        /* Check for Testnet3 (0x0709110B) or Testnet4 (0x1C163F28) */
        if (magic == 0x0709110B || magic == 0x1C163F28) {
            printf("SUCCESS: Found Magic Number 0x%08X at byte offset: %ld\n", magic, i);
            match_count++;
            
            /* Stop after 20 matches so we don't flood your PowerShell terminal */
            if (match_count >= 20) {
                printf("...\n[Stopping output after 20 matches. The XOR key works!]\n");
                break;
            }
        }
    }

    if (match_count == 0) {
        printf("FAILED: No magic numbers found. The XOR key might be wrong, or the file is empty.\n");
    }

    free(buf);
    return 0;
}