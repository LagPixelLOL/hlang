/* hash -- C mirror of hash.HC (same algorithm, same checksum). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE 0x100000
#define REPS 100

int main(void) {
    uint8_t* buf = malloc(SIZE);
    int64_t i, r;
    for (i = 0; i < SIZE; i++) buf[i] = (uint8_t)(i * 31 + 7);
    uint64_t h = 0xCBF29CE484222325ULL;
    for (r = 0; r < REPS; r++)
        for (i = 0; i < SIZE; i++) {
            h = h ^ buf[i];
            h = h * 0x100000001B3ULL;
        }
    printf("%llX\n", (unsigned long long)h);
    free(buf);
    return 0;
}
