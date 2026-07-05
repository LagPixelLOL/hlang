// EDGE: numeric literal & boundary behavior.
// Doc-grade: hex (0x...), decimal, char consts ('A'==65, 'ABC'==0x434241),
// F64 (no F32). 0b binary literals are an hcc EXTENSION (not doc-attested
// HolyC) -- asserted below as hcc-defined behavior, see README. '_' digit
// separators are likewise an hcc extension and deliberately NOT exercised.

// signed 64-bit boundaries (prelude consts)
"%d\n",I64_MAX;              // 9223372036854775807
"%d\n",I64_MIN;             // -9223372036854775808
"%X\n",I64_MAX;            // 7FFFFFFFFFFFFFFF
"%X\n",I64_MIN;           // 8000000000000000

// unsigned 64-bit boundary, three views
"%u\n",U64_MAX;             // 18446744073709551615
"%d\n",U64_MAX;            // -1
"%X\n",U64_MAX;           // FFFFFFFFFFFFFFFF

// narrower boundary consts
"%d %d %d\n",I8_MAX,I16_MAX,I32_MAX;      // 127 32767 2147483647
"%d %d %d\n",U8_MAX,U16_MAX,U32_MAX;      // 255 65535 4294967295
"%d %d\n",I8_MIN,I16_MIN;                 // -128 -32768

// hex: case-insensitive digits, various widths, negative
"%d %d\n",0xff,0xFF;                       // 255 255
"%d\n",0xDEADBEEF;                        // 3735928559
"%d\n",-0x80000000;                      // -2147483648
"%X\n",0x0;                             // 0
"%d\n",0x7FFFFFFFFFFFFFFF;              // 9223372036854775807

// binary literals (0b) -- hcc extension, hcc-defined
"%d %d %d\n",0b0,0b1,0b1010;              // 0 1 10
"%d\n",0b11111111;                       // 255
"%X\n",0b1111000011110000;              // F0F0

// char constants as integers, little-endian packing
"%d\n",'A';                               // 65
"%d\n",'0';                               // 48
"%X\n",'ABC';                            // 434241
"%X\n",'\n';                            // A
"%X\n",'12345678';                     // 3837363534333231 (8 bytes, max)

// decimal boundary / large values
"%d\n",1000000000000;                     // 10^12 fits I64
"%d\n",9223372036854775807;              // I64_MAX as decimal
"%u\n",18446744073709551615;            // U64_MAX as decimal

// F64 literals (no F32 in HolyC), incl scientific notation
"%5.2f\n",3.14159;                        //  3.14
"%5.1f\n",1.5e3;                          // 1500.0
"%8.6f\n",0.5;                            // 0.500000
"%5.1f\n",2.5e-1*10;                      // 2.5
"%g\n",1e10;                              // 1e+10

// mixing bases in one expression
"%d\n",0xF+0b1+9+'\x01';                 // 15+1+9+1 = 26

// boundary arithmetic (wraps, from arith test but as literals)
"%X\n",0x7FFFFFFFFFFFFFFF+1;             // 8000000000000000
"%u\n",0xFFFFFFFFFFFFFFFF+1;            // 0
