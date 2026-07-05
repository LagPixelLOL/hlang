// EDGE: functions -- defaults anywhere, skipped args, no-paren calls,
// empty/large varargs, recursion+varargs, function-pointer arrays.
// (All patterns are doc-attested: default args & Test(,3) from README,
//  varargs argc/argv, fn-ptr members from ClassMeta.HC.)

// defaults not on the end + skipped args (README's exact example, extended)
U0 T(I64 i=4,I64 j=0,I64 k=5)
{
  "%d %d %d\n",i,j,k;
}
T(,3);                       // 4 3 5
T;                           // 4 0 5  (no-paren, all defaults)
T();                         // 4 0 5
T(,,);                       // 4 0 5  (all slots skipped)
T(1,2,3);                    // 1 2 3
T(9);                        // 9 0 5
T(,,9);                      // 4 0 9

// a default that is an expression
I64 Dozen(I64 n=6*2) { return n; }
"%d %d\n",Dozen,Dozen(1);    // 12 1

// varargs: empty, one, many
I64 Sum(...)
{
  I64 i,s=0;
  for (i=0;i<argc;i++) s+=argv[i];
  return s;
}
"%d %d %d %d\n",Sum,Sum(5),Sum(1,2,3),Sum(1,2,3,4,5,6,7,8,9,10);  // 0 5 6 55

// named params before varargs
I64 Weighted(I64 w,...)
{
  I64 i,s=0;
  for (i=0;i<argc;i++) s+=argv[i];
  return w*s;
}
"%d\n",Weighted(3);          // 3*0 = 0 (empty varargs)
"%d\n",Weighted(3,1,2,3);    // 3*6 = 18

// recursion combined with varargs (each frame has its own argc/argv)
I64 SumAll(...)
{
  I64 i,s=0;
  for (i=0;i<argc;i++) s+=argv[i];
  return s;
}
I64 Chain(I64 depth)
{
  if (!depth) return 0;
  return SumAll(depth,depth,depth)+Chain(depth-1);   // 3*depth + ...
}
"%d\n",Chain(4);             // 3*(4+3+2+1) = 30

// forwarding varargs the TempleOS way (StrPrintJoin)
U8 *Fmt(U8 *f,...) { return StrPrintJoin(NULL,f,argc,argv); }
U8 *s=Fmt("%d/%d/%s",1,2,"go");
"%s\n",s;
Free(s);

// function-pointer array via fn-ptr members (doc: ClassMeta.HC)
I64 Add(I64 a,I64 b) { return a+b; }
I64 Sub(I64 a,I64 b) { return a-b; }
I64 Mul(I64 a,I64 b) { return a*b; }
class Op { I64 (*fn)(I64,I64); U8 *name; };
Op ops[3];
ops[0].fn=&Add; ops[0].name="add";
ops[1].fn=&Sub; ops[1].name="sub";
ops[2].fn=&Mul; ops[2].name="mul";
I64 i;
for (i=0;i<3;i++) "%s(7,3)=%d\n",ops[i].name,ops[i].fn(7,3);

// function addresses are I64s: store in an I64 array, call via typed fn-ptr
I64 addrs[3];
addrs[0]=&Add; addrs[1]=&Sub; addrs[2]=&Mul;
I64 (*fp)(I64,I64);
for (i=0;i<3;i++) { fp=addrs[i]; "%d ",fp(20,4); }
'\n';                        // 24 16 80

// &Fun for callbacks; call through the pointer directly and via Call()
U0 Hi() { "hi\n"; }
I64 apfp=&Hi;
Call(apfp);                  // hi

// default arg is evaluated at the CALL site (sees current global value)
I64 base=10;
I64 FromBase(I64 v=base) { return v; }
"%d\n",FromBase;             // 10
base=99;
"%d\n",FromBase;             // 99
