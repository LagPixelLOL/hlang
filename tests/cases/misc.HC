// grab-bag of accepted-keyword surface from the docs: no_warn, reg/noreg
// (register allocation belongs to LLVM, so 'reg R15' pinning parses and
// is ignored -- documented limitation), function flags, lock (with and
// without braces, per Lock.HC), Option() no-ops, zero-size I0/U0.

// function flags parse (hosted no-ops: no IRQs, no err codes to pop)
interrupt U0 IrqStub() { }
haserrcode U0 ErrStub() { }
argpop U0 Ap() { }
noargpop U0 Nap() { }
public U0 Pub() { "pub\n"; }
static U0 Priv() { "priv\n"; }
Pub;
Priv;

// reg (optionally with a named register), noreg, no_warn
U0 RegDemo()
{
  I64 reg R15 i=5, noreg j=4;
  no_warn i;
  "%d %d\n",i,j;
}
RegDemo;

// lock{} compiles its body (single-threaded hosted runtime -- no LOCK
// prefixes, documented); Lock.HC shows it "Can be used without {}"
I64 glbl=0;
lock { glbl++; }
lock glbl++;
"%d\n",glbl;

// Option() is accepted (and meaningless here) with any OPTf_* flag
Option(OPTf_WARN_DUP_TYPES,ON);
Option(OPTf_NO_REG_VAR,OFF);
"opt ok\n";

// I0/U0 are real (zero-size) types, per the type table
"%d %d\n",sizeof(I0),sizeof(U0);
