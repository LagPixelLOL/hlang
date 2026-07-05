// Variable arg count functions access args with 'I64 argc' and 'I64 argv[]'
I64 AddNums(...)
{
  I64 i,res=0;
  for (i=0;i<argc;i++)
    res+=argv[i];
  return res;
}
"%d\n",AddNums(1,2,3);
"%d\n",AddNums;
"%d\n",AddNums(10);

// named params + varargs
I64 SumScaled(I64 scale,...)
{
  I64 i,res=0;
  for (i=0;i<argc;i++)
    res+=argv[i];
  return scale*res;
}
"%d\n",SumScaled(2, 1,2,3,4);

// forwarding varargs TempleOS-style (StrPrintJoin)
U8 *Fmt(U8 *fmt,...)
{
  return StrPrintJoin(NULL,fmt,argc,argv);
}
U8 *s=Fmt("x=%d y=%s",99,"str");
"%s\n",s;
Free(s);

// F64 through varargs (bit-cast into 64-bit slots like TempleOS stack args)
U0 P(U8 *fmt,...)
{
  "" fmt,argv[0],argv[1];
}
P("%5.2f %d\n",3.25,7);
