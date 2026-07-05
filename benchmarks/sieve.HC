// sieve -- branchy byte-array marking, repeated.
#define LIMIT 200000
#define REPS  50

U8 *flags=MAlloc(LIMIT+1);
I64 r,i,j,cnt,total=0;
for (r=0;r<REPS;r++) {
  MemSet(flags,1,LIMIT+1);
  cnt=0;
  for (i=2;i<=LIMIT;i++)
    if (flags[i]) {
      cnt++;
      for (j=i*i;j<=LIMIT;j+=i)
        flags[j]=0;
    }
  total+=cnt;
}
"%d\n",total;
Free(flags);
