// helloworld.HC variants from the docs
"Hello World\n";

U0 Main()
{
  "Hello World 2\n";
}
Main;

U0 MyPrint(U8 *st)
{
  "%s",st;
}
MyPrint("Hello World 3\n");

U0 MyPrint2(U8 *st1,U8 *st2)
{
  "%s %s\n",st1,st2;
}
MyPrint2("Hello","World");

U0 MyPrint3(U8 *st)
{
  "" st;  //Empty with no comma means first is fmt str.
}
MyPrint3("Hello World 5\n");

// char consts to PutChars
'H'; 'i'; '\n';
'Hello ';       //multi-char const
'World\n';
U8 drv='C';
'' drv; ':';'\n';
PutChars('ABC');
'\n';
// adjacent string concatenation
"con" "cat %d"
    "enation\n",1;
