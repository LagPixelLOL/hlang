// ANTI-C: default arguments. C has none, and C++ semantics don't apply:
// a HolyC default arg is a plain expression compiled into each CALL SITE.
// TempleOS itself relies on this -- graphics functions default to the
// LIVE global device context (U0 GrLine(CDC *dc=gr_dc,...)), so defaults
// must see current state, not a value frozen at definition time.
I64 base=10;
I64 FromBase(I64 v=base) { return v; }
"%d\n",FromBase;             // 10
base=99;
"%d\n",FromBase;             // 99

// a default may CALL a function, re-evaluated on every defaulted call
I64 n=0;
I64 Next() { return ++n; }
I64 Take(I64 v=Next) { return v; }
"%d ",Take; "%d ",Take; "%d\n",Take;   // 1 2 3
"%d\n",Take(100);            // 100: supplied arg suppresses the default...
"%d\n",n;                    // 3:   ...so Next was NOT run

// not constant-folded at definition time either
I64 g=1;
I64 Mul(I64 a,I64 b=g*10) { return a*b; }
"%d\n",Mul(2);               // 20
g=5;
"%d\n",Mul(2);               // 100

// skipped-arg slots take the call-site default too (doc: Test(,3))
I64 Pair(I64 x=base,I64 y) { return x*1000+y; }
"%d\n",Pair(,7);             // 99007
