/* fib -- C mirror of fib.HC (same algorithm, same checksum). */
#include <stdint.h>
#include <stdio.h>

static int64_t Fib(int64_t n) {
    if (n < 2) return n;
    return Fib(n - 1) + Fib(n - 2);
}

int main(void) {
    printf("%lld\n", (long long)Fib(34));
    return 0;
}
