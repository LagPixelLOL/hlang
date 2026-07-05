// Primes.HC -- sieve of Eratosthenes on a bitmap, using the TempleOS
// bit-test routines instead of shifts and masks.
//
//   $ hcc examples/Primes.HC
//
// Bt/Bts/Btr/Btc wrap the x86 BT/BTS/BTR/BTC instructions: test a bit,
// and (for the last three) set/reset/complement it in the same breath,
// returning what it WAS. Terry used them everywhere; so do we.

#define LIMIT 1000

// CAlloc gives zeroed memory, so every number starts "maybe prime".
// One bit per number: LIMIT/8+1 bytes is all the sieve costs.
U8 *sieve=CAlloc(LIMIT/8+1);

// Note the argument: Bt() is declared as Bt(U0 *bit_field,I64 bit_num)
// and we pass a U8* without any cast ceremony. HolyC has NO
// type-checking -- pointers are just I64s, and the doc wears that as a
// badge ("No type-checking"). Freedom, with the safety off.
I64 i,j,cnt=0;
for (i=2;i<=LIMIT;i++) {
  if (!Bt(sieve,i)) {           // bit clear -> i survived every sieve pass
    cnt++;
    "%5d",i;
    if (!(cnt%13)) '\n';        // 13 columns, why not
    // cross off the multiples, starting at i*i (the smaller ones were
    // already handled by smaller primes). Bts SETS the bit.
    for (j=i*i;j<=LIMIT;j+=i)
      Bts(sieve,j);
  }
}
'\n';
"%d primes <= %d\n",cnt,LIMIT;

// BCnt is a popcount -- count the crossed-off composites for fun.
// MSize() reports the allocation's true size (a real TempleOS function:
// the heap rounds up, so Terry gave you a way to use the slack).
I64 k,composites=0;
for (k=0;k<MSize(sieve);k++)
  composites+=BCnt(sieve[k]);
"%d numbers were crossed off the tablet.\n",composites;

Free(sieve);
