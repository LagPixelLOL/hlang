// ANTI-C: '%' on F64. C rejects the modulus operator on floating operands
// (C11 6.5.5p2 demands integer types) and makes you call fmod(). HolyC has
// one numeric tower -- I64/F64 with promotion -- and hcc lowers a float %
// to fmod (doc-silent corner, documented in the README).
"%5.3f\n",7%2.5;             // 2.000 (int % float promotes)
"%5.3f\n",7.5%2;             // 1.500
F64 a=10.75,b=3.5;
"%5.3f\n",a%b;               // 0.250
"%5.3f\n",-7.5%2;            // -1.500 (sign follows the dividend, fmod-style)
I64 n=9;
"%5.3f\n",n%2.5;             // 1.500
"%d\n",9%4;                  // 1 (pure ints stay integral)
