// EDGE: scoping per the HolyC spec (scoping table): local vars are
// FunctionScope + NoDups. A local may overshadow a GLOBAL (globals are
// DupsAllowed), but you may NOT declare the same local name twice in one
// function -- unlike C, there is no block-level shadowing. (See
// edge_dup_local.HC for the rejected case.)

I64 g=100;

// a local overshadows the global of the same name
U0 Shadow()
{
  I64 g=5;                   // overshadows global g (allowed: globals dup)
  "%d\n",g;                  // 5
  g=8;
  "%d\n",g;                  // 8
}
Shadow;
"%d\n",g;                    // 100 (global untouched)

// writing the global directly (no local of that name) persists
U0 Touch() { g=200; }
Touch;
"%d\n",g;                    // 200

// static locals persist across calls; initialized once
I64 Counter()
{
  static I64 n=0;
  n++;
  return n;
}
"%d %d %d %d\n",Counter,Counter,Counter,Counter;   // 1 2 3 4

// independent statics within one function
U8 *Tick()
{
  static I64 t=10;
  static I64 calls=0;
  t+=5;
  calls++;
  return MStrPrint("t=%d calls=%d",t,calls);
}
U8 *s1=Tick; "%s\n",s1; Free(s1);
U8 *s2=Tick; "%s\n",s2; Free(s2);

// distinct names in nested blocks are fine
U0 Distinct()
{
  I64 outer=1;
  {
    I64 inner=outer+1;
    "%d %d\n",outer,inner;   // 1 2
  }
}
Distinct;

// a param is a local; reuse the SAME var (idiomatic HolyC), don't redeclare
I64 Doubler(I64 v)
{
  v=v*2;                     // reassign, not redeclare
  return v+1;
}
"%d\n",Doubler(21);          // 43

// one loop counter reused across loops (idiomatic: declare once)
I64 i,acc=0;
for (i=0;i<5;i++) acc+=i;    // 10
for (i=0;i<3;i++) acc+=i;    // +3
"%d\n",acc;                  // 13
