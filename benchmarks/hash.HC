// hash -- FNV-1a over a 1 MiB buffer, many passes: byte loads, 64-bit
// multiply wraparound (two's complement, same in both languages).
#define SIZE 0x100000
#define REPS 100

U8 *buf=MAlloc(SIZE);
I64 i,r;
for (i=0;i<SIZE;i++)
  buf[i]=i*31+7;                  // truncating byte store
U64 h=0xCBF29CE484222325;
for (r=0;r<REPS;r++)
  for (i=0;i<SIZE;i++) {
    h=h^buf[i];
    h=h*0x100000001B3;
  }
"%X\n",h;
Free(buf);
