/* matmul -- C mirror of matmul.HC (same algorithm, same checksum). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define N 256

int main(void) {
    double* a = malloc(N * N * 8);
    double* b = malloc(N * N * 8);
    double* c = malloc(N * N * 8);
    int64_t i, j, k;
    for (k = 0; k < N * N; k++) {
        a[k] = ((k * 7) & 1023) * 0.001953125;   /* n/512 */
        b[k] = ((k * 13) & 1023) * 0.0009765625; /* n/1024 */
    }
    double sum;
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            sum = 0;
            for (k = 0; k < N; k++) sum += a[i * N + k] * b[k * N + j];
            c[i * N + j] = sum;
        }
    double chk = 0;
    for (k = 0; k < N * N; k++) chk += c[k];
    printf("%lld\n", (long long)chk);
    free(a);
    free(b);
    free(c);
    return 0;
}
