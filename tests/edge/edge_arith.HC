// EDGE: integer overflow / wraparound / 64-bit widening boundaries.
// All two's-complement wraparound; everything extends to 64-bit on access.

// signed overflow wraps
"%d\n",I64_MAX+1;              // I64_MIN = -9223372036854775808
"%X\n",I64_MAX+1;             // 8000000000000000
"%d\n",I64_MIN-1;             // wraps to I64_MAX = 9223372036854775807
"%d\n",-I64_MIN;             // negating I64_MIN wraps to itself
"%X\n",-I64_MIN;            // 8000000000000000

// unsigned wraparound
U64 u=U64_MAX;
"%u\n",u+1;                   // 0
"%d\n",u;                     // -1 (as signed)
"%u\n",u;                     // 18446744073709551615
"%X\n",0(U64)-1;             // FFFFFFFFFFFFFFFF

// multiply overflow wraps to zero
"%d\n",0x100000000*0x100000000;   // 2^64 -> 0
"%X\n",0xFFFFFFFFFFFFFFFF*2;     // FFFFFFFFFFFFFFFE

// shift boundaries
"%X\n",1<<63;                 // 8000000000000000
"%d\n",1<<63;                 // I64_MIN
I64 sneg=1<<63;
"%X\n",sneg>>63;             // arithmetic: FFFFFFFFFFFFFFFF
U64 uneg=1<<63;
"%X\n",uneg>>63;            // logical: 1
"%X\n",(1<<63)>>63;         // whole expr is I64: FFFFFFFFFFFFFFFF

// x86/HolyC shift-count masking: shift by 64 == shift by 0 (mod 64)
I64 one=1;
"%d\n",one<<64;              // 1  (count masked to 0)
"%d\n",one<<65;             // 2  (count masked to 1)
I64 val=0x1234;
"%X\n",val>>64;            // 1234 (no shift)

// narrow-type wraparound on access (stored at physical width)
I8 c8=127;
c8++;
"%d\n",c8;                    // -128
U8 b8=255;
b8++;
"%d\n",b8;                    // 0
I16 s16=0x7FFF;
s16+=1;
"%d\n",s16;                   // -32768
// assignment truncates to declared width, then sign/zero-extends on read
I8 t=300;
"%d\n",t;                     // 300 & 0xFF = 44
U8 v=-1;
"%d\n",v;                     // 255
I16 w=0x1FFFF;
"%d\n",w;                     // 0x1FFFF & 0xFFFF = -1 (0xFFFF signed)

// intermediate calcs are 64-bit even for narrow operands
I8 a1=100,a2=100;
"%d\n",a1*a2;                 // 10000 (not truncated mid-expression)
"%d\n",a1+a2+a1;              // 300
