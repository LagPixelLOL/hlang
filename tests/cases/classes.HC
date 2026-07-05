// classes, unions, single inheritance, sizeof/offset, lastclass, fn ptrs
class CPoint
{
  I64 x,y;
};
class CPoint3:CPoint
{
  I64 z;
};
CPoint3 p;
p.x=1; p.y=2; p.z=3;
"%d %d %d\n",p.x,p.y,p.z;
"%d %d %d\n",sizeof(CPoint),sizeof(CPoint3),offset(CPoint3.z);
"%d %d\n",sizeof(CPoint3.z),sizeof(CPoint.x);

// mixed member sizes: HolyC packs members (no C-style alignment padding;
// TempleOS pads by hand with 'pad'/'reserved' members). See tests/anti_c/.
class CMix
{
  U8 a;
  I16 b;
  U8 name[5];
  I64 c;
};
"%d %d %d %d\n",offset(CMix.b),offset(CMix.name),offset(CMix.c),sizeof(CMix);

// plain union
union UAll
{
  I64 whole;
  U8 bytes[8];
};
UAll u;
u.whole=0x4142434445464748;
"%c %c %d\n",u.bytes[0],u.bytes[7],sizeof(UAll);

// instances declared after class body; pointers, ->, nested classes
class Student
{
  Student *next;
  I64 age;
  U8 name[32];
} a;
class School
{
  U8 name[32];
  Student *students;
} s;
a.age=19;
StrCpy(a.name,"Terry");
s.students=&a;
StrCpy(s.name,"Temple High");
"%s: %s is %d\n",s.name,s.students->name,s.students->age;
a.next=&a;
"%d\n",a.next->next->age;

// lastclass
U0 StructName(U8 *d,U8 *class_name=lastclass)
{
  "\"%s\"\n",class_name;
}
I64 ii;
F64 ff;
StructName(&a);
StructName(&s);
StructName(&ii);
StructName(&ff);
StructName(Fs);

// member meta data parses (reflection not supported; values ignored)
class TMeta
{
  I64 age print_str "%2d" dft_val 38;
  I64 rank print_str "%1d" dft_val 6/2 percentile 54.20;
};
TMeta t;
t.age=44;
"%d %d\n",t.age,sizeof(TMeta);

// function pointer members
class CB
{
  I64 (*fp)(I64 a,I64 b);
};
I64 AddFn(I64 a,I64 b) { return a+b; }
CB cb;
cb.fp=&AddFn;
"%d\n",cb.fp(3,4);
// fn ptr local var + Call()
U0 Hello() { "hi from ptr\n"; }
U0 (*g)();
g=&Hello;
g();
Call(&Hello);
(*g)();
