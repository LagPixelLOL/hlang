/* sieve -- C mirror of sieve.HC (same algorithm, same checksum). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIMIT 200000
#define REPS 50

int main(void) {
    uint8_t* flags = malloc(LIMIT + 1);
    int64_t r, i, j, cnt, total = 0;
    for (r = 0; r < REPS; r++) {
        memset(flags, 1, LIMIT + 1);
        cnt = 0;
        for (i = 2; i <= LIMIT; i++)
            if (flags[i]) {
                cnt++;
                for (j = i * i; j <= LIMIT; j += i) flags[j] = 0;
            }
        total += cnt;
    }
    printf("%lld\n", (long long)total);
    free(flags);
    return 0;
}
