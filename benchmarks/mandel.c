/* mandel -- C mirror of mandel.HC (same algorithm, same checksum). */
#include <stdint.h>
#include <stdio.h>

#define W 1600
#define H 800
#define MAXIT 256

int main(void) {
    int64_t x, y, it, total = 0;
    double cr, ci, zr, zi, t;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            cr = -2.5 + x * 3.5 / W;
            ci = -1.25 + y * 2.5 / H;
            zr = 0;
            zi = 0;
            it = 0;
            while (it < MAXIT && zr * zr + zi * zi < 4.0) {
                t = zr * zr - zi * zi + cr;
                zi = 2 * zr * zi + ci;
                zr = t;
                it++;
            }
            total += it;
        }
    printf("%lld\n", (long long)total);
    return 0;
}
