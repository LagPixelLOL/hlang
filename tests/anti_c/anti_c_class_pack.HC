// ANTI-C: class layout. C compilers insert alignment padding between
// members and round sizeof up to the widest alignment. HolyC does NOT:
// members are packed back-to-back and sizeof is the exact byte sum.
// TempleOS aligns by hand with 'pad'/'reserved' members (CDirEntry:
// "U16 attr; U8 pad[6],name[...];") -- which is exactly why the spec
// special-cases those two member names. Do not project C ABI padding.
class CPacked
{
  U8  a;      // @0
  I16 b;      // @1  (C would say @2)
  U8  c;      // @3
  I64 d;      // @4  (C would say @8)
  U8  e;      // @12
};
"%d %d %d %d %d\n",offset(CPacked.a),offset(CPacked.b),offset(CPacked.c),
    offset(CPacked.d),offset(CPacked.e);
"%d\n",sizeof(CPacked);      // 13 (C: 24)

// unaligned member loads/stores just work
CPacked p;
p.a=1; p.b=-2; p.c=3; p.d=0x123456789A; p.e=5;
"%d %d %d %X %d\n",p.a,p.b,p.c,p.d,p.e;

// inheritance keeps packing: derived members start at the base's exact size
class CKid:CPacked { I16 f; };
"%d %d\n",offset(CKid.f),sizeof(CKid);     // 13 15

// manual padding, the TempleOS way; multiple 'pad' members are allowed
class CManual
{
  U16 attr;
  U8  pad[6];    // hand-aligned, like CDirEntry
  I64 big;
  U8  pad[4];    // second 'pad' member: fine per the spec
};
"%d %d\n",offset(CManual.big),sizeof(CManual);   // 8 20

// arrays of packed classes stride by the packed size
CPacked arr[3];
arr[0].e=10; arr[1].e=20; arr[2].e=30;
"%d %d %d\n",arr[0].e,arr[1].e,arr[2].e;
"%d\n",sizeof(arr);          // 39 (C: 72)
