// console input: GetStr / GetI64 / GetF64 / YorN / GetChar
U8 *name=GetStr("Name:");
"Hello %s!\n",name;
Free(name);
I64 n=GetI64("Num:",42,0,100);
"n=%d\n",n;
I64 d=GetI64("Dft:",42,0,100);   //empty line -> default
"d=%d\n",d;
F64 f=GetF64("F:",1.0);
"f=%3.1f\n",f;
if (YorN) "yes!\n"; else "no!\n";
"ch=%c\n",GetChar;
