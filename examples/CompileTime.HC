// CompileTime.HC -- #exe{}: run HolyC while COMPILING HolyC.
//
//   $ hcc examples/CompileTime.HC
//
// In TempleOS the compiler is always resident, so Terry gave the
// preprocessor a block that JIT-compiles and RUNS its contents at
// compile time; whatever the block StreamPrint()s is spliced into the
// source stream, exactly where the block stood. It is a macro system
// whose macro language is... the whole language.

// ---------------------------------------------------- generated tables
// The initializer list below does not exist in this file: the compiler
// writes it while reading this line. 16 factorials, computed once at
// build time, costing the running program nothing.
I64 fact[16]={
#exe {
  I64 i,f=1;
  for (i=0;i<16;i++) {
    if (i) f*=i;
    StreamPrint("%d,",f);
  }
}
};

"12! = %d (from the table the compiler wrote)\n",fact[12];

// ------------------------------------------------- generated functions
// StreamPrint can emit ANY source text -- including whole functions.
#exe {
  I64 p;
  for (p=0;p<4;p++)
    StreamPrint("I64 Times%d(I64 x) { return x*%d; }\n",1<<p,1<<p);
}
// Times1..Times8 now exist as if you had typed them:
"%d %d %d %d\n",Times1(21),Times2(21),Times4(21),Times8(21);

// ------------------------------------------------ compile-time consts
// __DATE__/__TIME__ and friends are #exe in disguise -- TempleOS defines
// them as '#define __TIME__ #exe{StreamPrint("\"%T\"",Now);}'.
"built on %s at %s\n",__DATE__,__TIME__;
"source file: %s, line %d\n",__FILE__,__LINE__;

// -------------------------------------------------- conditional splice
// #exe can decide what code even EXISTS, e.g. by compilation mode
// (#ifjit/#ifaot select on JIT vs AOT -- also usable directly):
#ifjit
"this sentence was JIT-compiled into memory and ran immediately.\n";
#endif
#ifaot
"this sentence was compiled ahead of time into a real executable.\n";
#endif

// -------------------------------------------------------- one more toy
// Precompute a string at build time: the compiler unrolls the psalm.
U8 *psalm=
#exe {
  I64 k;
  StreamPrint("\"");
  for (k=1;k<=3;k++)
    StreamPrint("verse %d... ",k);
  StreamPrint("selah.\"");
}
;
"%s\n",psalm;
