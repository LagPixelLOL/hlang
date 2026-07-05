// EDGE: control-flow corners -- empty loops, goto spaghetti, do-while-once,
// break edges, dead code after return/goto, labels.

I64 i,n;

// empty-bodied loops that only run their header
n=0;
for (i=0;i<1000;i++) ;
"%d\n",i;                    // 1000
for (i=0;i<1000000;i++) n++;
"%d\n",n;                    // 1000000

// for with empty condition + break
i=0;
for (;;) { if (i>=5) break; i++; }
"%d\n",i;                    // 5

// for with empty init and post
i=0;
for (;i<3;) i++;
"%d\n",i;                    // 3

// while(0) never runs; do-while runs once
while (0) "never\n";
i=0;
do { i++; } while (0);
"%d\n",i;                    // 1
i=100;
do { "once %d\n",i; } while (i<0);   // body executes once despite false cond

// nested loops with labeled goto to break out of both
I64 found=-1;
for (i=0;i<10;i++)
  for (n=0;n<10;n++)
    if (i*n==12 && i<n) { found=i*100+n; goto done; }
done:
"%d\n",found;                // 3*100+4 = 304

// goto spaghetti: forward and backward jumps
i=0;
goto mid;
top:
  i+=10;
  goto check;
mid:
  i+=1;
  goto top;
check:
  if (i<25) goto top;
"%d\n",i;                    // 1, +10=11, check<25 ->top +10=21 ->check<25 ->top+10=31 ->check>=25 stop = 31

// goto implementing the missing 'continue'
I64 sum=0;
i=0;
again:
  i++;
  if (i>10) goto stop;
  if (i&1) goto again;       // skip odds (this is HolyC 'continue')
  sum+=i;
  goto again;
stop:
"%d\n",sum;                  // 2+4+6+8+10 = 30

// dead code after return is fine (unreachable)
I64 Early(I64 x)
{
  if (x>0) return 1;
  return -1;
  "unreachable\n";           // never executed / valid
}
"%d %d\n",Early(5),Early(-5);

// break only affects innermost loop
I64 outer=0;
for (i=0;i<3;i++) {
  for (n=0;n<3;n++) {
    if (n==1) break;
    outer++;
  }
}
"%d\n",outer;                // 3 (one per outer iter)

// label immediately followed by end of function
U0 LabelEnd()
{
  "before\n";
  goto skip;
  "skipped\n";
skip:
}
LabelEnd;

// deeply nested blocks
{ { { { { "deep\n"; } } } } }
