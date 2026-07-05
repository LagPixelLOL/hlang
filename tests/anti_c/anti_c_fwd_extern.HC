// ANTI-C: forward references. C reaches for a bare prototype; the HolyC
// scoping doc says `extern` "binds a new HTT_FUN ... sym to an existing
// sym of the same name if it exists ... It also can be used to generate a
// fwd reference." This is the doc-attested spelling for mutual recursion.
extern Bool IsOdd(I64 n);

Bool IsEven(I64 n)
{
  if (!n) return TRUE;
  return IsOdd(n-1);
}
Bool IsOdd(I64 n)
{
  if (!n) return FALSE;
  return IsEven(n-1);
}
"%d %d\n",IsEven(10),IsOdd(7);   // 1 1
"%d %d\n",IsEven(1),IsOdd(0);    // 0 0

// extern fwd reference to a global, defined (and initialized) later
extern I64 answer;
I64 Get() { return answer; }
I64 answer=42;
"%d\n",Get;                      // 42
