// "All values are extended to 64-bit when accessed" -- exact doc examples
U0 Main()
{
  I16 i1;
  I32 j1;
  j1=i1=0x12345678;           //Resulting i1 is 0x5678 but j1 is 0x12345678
  "%X %X\n",i1,j1;

  I64 i2=0x8000000000000000;
  Print("%X\n",i2>>1);        //Res is 0xC000000000000000 as expected

  U64 u3=0x8000000000000000;
  Print("%X\n",u3>>1);        //Res is 0x4000000000000000 as expected

  I32 i4=0x80000000;          //const is loaded into a 64-bit reg var.
  Print("%X\n",i4>>1);        //Res is 0x40000000

  I32 i5=-0x80000000;
  Print("%X\n",i5>>1);        //Res is 0xFFFFFFFFC0000000
}
Main;

// memory (globals/members) keeps physical widths
I16 g16;
U16 gu16;
g16=0x12345678;
gu16=0xFEDC;
"%X %X\n",g16,gu16;      //5678, FEDC (zero-extended: unsigned)
g16=-1;
"%d %X\n",g16,g16;       //-1 FFFFFFFFFFFFFFFF (sign-extended on access)

// U64 vs I64 division & comparison
I64 a=-8;
U64 b=-8;
"%d %d\n",a/2,a%3;
"%X\n",b/2;              //unsigned divide: 7FFFFFFFFFFFFFFC
"%d %d\n",a<2,b<2;       //signed: 1, unsigned: 0

// char consts
"%X\n",'ABC';            //0x434241
"%d\n",'A';
U8 small;
small=0x1FF;
"%d\n",small;            //truncated to 8 bits: 255
