// string runtime + Print format codes
"%s %d\n",StrNew("dup"),StrLen("hello");
U8 buf[64];
StrCpy(buf,"hello");
CatPrint(buf," %s","world");
"%s\n",buf;
StrPrint(buf,"n=%d",42);
"%s\n",buf;
U8 *m=MStrPrint("%s-%s","a","b");
"%s %d\n",m,MSize(m);
Free(m);
"%d %d %d\n",StrCmp("abc","abc"),StrCmp("abc","abd")<0,StrNCmp("abcdef","abcxyz",3);
"%d\n",StrICmp("HeLLo","hello");
"%s\n",StrFind("na","banana");
"%d %d\n",Str2I64("123"),Str2I64("ff",16);
"%5.2f\n",Str2F64("2.5");
"%d %d\n",StrOcc("banana",'a'),StrOcc("banana",'q');
// fmt codes
"%X %x %u\n",255,255,18446744073709551615;
"[%5d][%-5d][%05d]\n",42,42,42;
"%,d\n",1234567;
"%c%c\n",'AB','!';
"[%8.3f][%e]\n",3.14159,12345.678;
"%z %z\n",1,"Cadet\0Ensign\0Captain\0",2,"Cadet\0Ensign\0Captain\0";
"%Q\n","quote\"back\\slash";
"%P\n",0x1234;
"100%% sure\n";
"%s\n",NULL(U8 *);
// escapes & $$
"tab\there $$5\n";
