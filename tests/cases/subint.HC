// Sub-int access (SubIntAccess.HC) + typed unions
I64 q=0xFEDCBA9876543210,q1;
"q\t\t=%016X\n",q;
q1=q.i16[2];
"q.i16[2]\t=%016X\n",q1;
q1=q.u8[5];
"q.u8[5]\t\t=%016X\n",q1;
q1=q.i32[1].u8[2];
"q.i32[1].u8[2]\t=%016X\n",q1;
q1=q.i32[0].i8[1];
"q.i32[0].i8[1]\t=%016X\n",q1;

// writes through sub-ints (README example)
I64 i=0x123456780000DEF0;
i.u16[1]=0x9ABC;
"%X\n",i;
i.u8[7]=0;
"%X\n",i;

// user-defined typed union: type in front is the whole-access type
public I64i union MyI64
{
  I8i i8[8];
  U8i u8[8];
  I16 i16[4];
  U16 u16[4];
  I32 i32[2];
  U32 u32[2];
};
MyI64 m;
m=0x1122334455667788;
"%X %X %X\n",m,m.u16[3],m.u8[0];
m.i32[0]=-1;
"%X\n",m;
"%d\n",sizeof(MyI64);

// dynamic index
I64 v=0x0807060504030201,k,sum=0;
for (k=0;k<8;k++)
  sum+=v.u8[k];
"%d\n",sum;
