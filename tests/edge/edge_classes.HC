// EDGE: deep class inheritance, union type-punning, nested member+array
// chains, arrays of classes, pointers to members.

// deep single-inheritance chain (spec: only one base class allowed)
class A       { I64 a; };
class B : A   { I64 b; };
class C : B   { I64 c; };
class D : C   { I64 d; };
D obj;
obj.a=1; obj.b=2; obj.c=3; obj.d=4;
"%d %d %d %d\n",obj.a,obj.b,obj.c,obj.d;              // 1 2 3 4
"%d %d %d %d\n",sizeof(A),sizeof(B),sizeof(C),sizeof(D);   // 8 16 24 32
"%d %d %d %d\n",offset(D.a),offset(D.b),offset(D.c),offset(D.d);  // 0 8 16 24

// base-typed pointer viewing a derived object (prefix layout)
A *pa=&obj(A *);
"%d\n",pa->a;                                         // 1 (shared prefix)

// union type-punning: write one member, read another
union U
{
  I64 whole;
  U32 halves[2];
  U8 bytes[8];
};
U u;
u.whole=0x1122334455667788;
"%X %X\n",u.halves[0],u.halves[1];                    // 55667788 11223344
"%X %X\n",u.bytes[0],u.bytes[7];                      // 88 11
u.bytes[0]=0xFF;
"%X\n",u.whole;                                       // ...55667788 -> ..FF

// float/int punning through a union
union FI { F64 f; I64 i; U64 bits; };
FI fi;
fi.f=1.0;
"%X\n",fi.bits;                                        // 3FF0000000000000

// nested classes by value + member+array chains
class Inner { I64 vals[4]; U8 tag; };
class Outer { Inner items[3]; I64 count; };
Outer o;
I64 i,j;
for (i=0;i<3;i++) {
  for (j=0;j<4;j++)
    o.items[i].vals[j]=i*10+j;
  o.items[i].tag='A'+i;
}
o.count=3;
"%d %d %d\n",o.items[0].vals[0],o.items[1].vals[2],o.items[2].vals[3];  // 0 12 23
"%c %c %c %d\n",o.items[0].tag,o.items[1].tag,o.items[2].tag,o.count;   // A B C 3
"%d %d\n",sizeof(Inner),sizeof(Outer);

// pointer into a nested member, chained arrows
Inner *pin=&o.items[1];
"%d %c\n",pin->vals[2],pin->tag;                      // 12 B

// array of pointers to classes
A a0,a1,a2;
a0.a=100; a1.a=200; a2.a=300;
A *arr[3];
arr[0]=&a0; arr[1]=&a1; arr[2]=&a2;
I64 tot=0;
for (i=0;i<3;i++) tot+=arr[i]->a;
"%d\n",tot;                                           // 600

// self-referential member chain (linked via value+pointer)
class Tree { Tree *left,*right; I64 v; };
Tree n0,n1,n2;
n0.v=1; n1.v=2; n2.v=3;
n0.left=&n1; n0.right=&n2;
n1.left=NULL; n1.right=NULL;
n2.left=NULL; n2.right=NULL;
"%d %d %d\n",n0.v,n0.left->v,n0.right->v;             // 1 2 3
"%d\n",n0.left->left==NULL;                           // 1

// sub-int access on a class member that is an integer
class Reg { I64 raw; };
Reg r;
r.raw=0xAABBCCDD;
"%X %X\n",r.raw.u8[0],r.raw.u16[1];                   // DD AABB
