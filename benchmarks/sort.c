/* sort -- C mirror of sort.HC (same algorithm, same checksum). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define N 0x200000

static void QSort(int64_t* a, int64_t lo, int64_t hi) {
    if (lo >= hi) return;
    int64_t p = a[(lo + hi) / 2], l = lo, r = hi, t;
    while (l <= r) {
        while (a[l] < p) l++;
        while (a[r] > p) r--;
        if (l <= r) {
            t = a[l];
            a[l] = a[r];
            a[r] = t;
            l++;
            r--;
        }
    }
    QSort(a, lo, r);
    QSort(a, l, hi);
}

int main(void) {
    int64_t* arr = malloc(N * 8);
    uint64_t seed = 88172645463325252ULL;
    int64_t i;
    for (i = 0; i < N; i++) {
        seed = seed ^ (seed << 13);
        seed = seed ^ (seed >> 7);
        seed = seed ^ (seed << 17);
        arr[i] = seed & 0xFFFFFF;
    }
    QSort(arr, 0, N - 1);
    int64_t chk = 0;
    for (i = 0; i < N; i++) chk += arr[i] * (i & 15);
    printf("%lld\n", (long long)chk);
    free(arr);
    return 0;
}
