// EDGE: deep recursion, mutual recursion, accumulators, Ackermann.

I64 Fact(I64 n)
{
  if (n<=1) return 1;
  return n*Fact(n-1);
}
"%d\n",Fact(20);             // 2432902008176640000 (fits in I64)

I64 Fib(I64 n)
{
  if (n<2) return n;
  return Fib(n-1)+Fib(n-2);
}
"%d\n",Fib(30);              // 832040

// mutual recursion (forward reference resolved by the pre-pass)
Bool IsOdd(I64 n);
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
"%d %d\n",IsEven(1000),IsOdd(1000);   // 1 0

// Ackermann (deep, non-tail)
I64 Ack(I64 m,I64 n)
{
  if (!m) return n+1;
  if (!n) return Ack(m-1,1);
  return Ack(m-1,Ack(m,n-1));
}
"%d %d\n",Ack(2,3),Ack(3,3);          // 9 61

// deep linear recursion (exercises the stack)
I64 Count(I64 n)
{
  if (!n) return 0;
  return 1+Count(n-1);
}
"%d\n",Count(50000);                   // 50000

// recursion via an accumulator argument (default arg seeds it)
I64 SumTo(I64 n,I64 acc=0)
{
  if (!n) return acc;
  return SumTo(n-1,acc+n);
}
"%d\n",SumTo(1000);                    // 500500

// recursion through a pointer parameter (self-referential update)
U0 Gather(I64 n,I64 *sum)
{
  if (!n) return;
  *sum=*sum+n;
  Gather(n-1,sum);
}
I64 total=0;
Gather(100,&total);
"%d\n",total;                          // 5050

// recursion returning F64
F64 PowN(F64 base,I64 e)
{
  if (!e) return 1.0;
  return base*PowN(base,e-1);
}
"%5.3f\n",PowN(2.0,10);                // 1024.000
