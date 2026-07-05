// MAlloc/CAlloc/MSize/MAllocIdent/Free + Mem* + bits
I64 *p=MAlloc(32);
"%d\n",MSize(p);
p[0]=11; p[1]=22; p[2]=33; p[3]=44;
"%d %d\n",p[0]+p[3],p[1]*p[2];
I64 *q=MAllocIdent(p);
"%d %d\n",q[2],MSize(q);
Free(q);
Free(p);
Free(NULL);          //You CAN Free() a NULL ptr.
U8 *z=CAlloc(16);
I64 i,sum=0;
for (i=0;i<16;i++) sum+=z[i];
"%d\n",sum;
MemSet(z,7,16);
"%d\n",z[0]+z[15];
U8 dst[8];
MemCpy(dst,"ABCDEFG",8);
"%s %d\n",dst,MemCmp(dst,"ABCDEFG",8);
Free(z);
// bit routines
I64 bits=0;
"%d",Bts(&bits,3);
"%d",Bt(&bits,3);
"%d",Bts(&bits,3);
"%d",Btr(&bits,3);
"%d",Bt(&bits,3);
"%d\n",Btc(&bits,60);
"%X %d\n",bits,BCnt(bits);
"%d %d\n",BCnt(0xFF),BCnt(0);
// Seed determinism
Seed(42);
I64 r1=RandU64;
Seed(42);
I64 r2=RandU64;
"%d\n",r1==r2;
Seed(43);
"%d\n",r1==RandU64;
