// matmul -- F64 dense matrix multiply (naive i-j-k).
// Inputs are exact dyadic fractions so every product and sum is exact in
// IEEE double; the truncating (I64) checksum is therefore bit-identical
// between HolyC and C.
#define N 256

F64 *a=MAlloc(N*N*8),*b=MAlloc(N*N*8),*c=MAlloc(N*N*8);
I64 i,j,k;
for (k=0;k<N*N;k++) {
  a[k]=((k*7)&1023)*0.001953125;      // n/512
  b[k]=((k*13)&1023)*0.0009765625;    // n/1024
}
F64 sum;
for (i=0;i<N;i++)
  for (j=0;j<N;j++) {
    sum=0;
    for (k=0;k<N;k++)
      sum+=a[i*N+k]*b[k*N+j];
    c[i*N+j]=sum;
  }
F64 chk=0;
for (k=0;k<N*N;k++)
  chk+=c[k];
"%d\n",chk(I64);
Free(a); Free(b); Free(c);
