// Default args don't have to be on the end + skipped args + no-paren calls
U0 Test(I64 i=4,I64 j,I64 k=5)
{
  Print("%X %X %X\n",i,j,k);
}
Test(,3);
Test(1,2,3);
Test(9,8);
Test(,7,);

U0 NoArgs()
{
  "noargs\n";
}
NoArgs;        //call without parens
NoArgs();

I64 AllDflt(I64 a=100,I64 b=23)
{
  return a+b;
}
"%d\n",AllDflt;             //no-paren call in expression
"%d\n",AllDflt+1;           //124
"%d\n",AllDflt(1);          //24
"%d\n",AllDflt(,1);         //101

// default arg referencing a global
I64 base=1000;
I64 WithGlobal(I64 v=base) { return v; }
"%d\n",WithGlobal;
base=2000;
"%d\n",WithGlobal;          //defaults evaluated at call site

// no-paren call as statement and as condition
I64 flag=0;
Bool Toggle() { flag=!flag; return flag; }
Toggle;
"%d\n",flag;
if (Toggle) "on\n"; else "off\n";
if (Toggle) "on\n"; else "off\n";
