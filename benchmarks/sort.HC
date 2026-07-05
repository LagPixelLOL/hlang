// sort -- quicksort of 2M I64s from a deterministic xorshift64 stream:
// comparisons, swaps, recursion.
#define N 0x200000

I64 *arr=MAlloc(N*8);
U64 seed=88172645463325252;
I64 i;
for (i=0;i<N;i++) {
  seed=seed^(seed<<13);
  seed=seed^(seed>>7);              // U64: logical shift, as in C
  seed=seed^(seed<<17);
  arr[i]=seed&0xFFFFFF;
}

U0 QSort(I64 *a,I64 lo,I64 hi)
{
  if (lo>=hi) return;
  I64 p=a[(lo+hi)/2],l=lo,r=hi,t;
  while (l<=r) {
    while (a[l]<p) l++;
    while (a[r]>p) r--;
    if (l<=r) {
      t=a[l]; a[l]=a[r]; a[r]=t;
      l++; r--;
    }
  }
  QSort(a,lo,r);
  QSort(a,l,hi);
}

QSort(arr,0,N-1);
I64 chk=0;
for (i=0;i<N;i++)
  chk+=arr[i]*(i&15);
"%d\n",chk;
Free(arr);
