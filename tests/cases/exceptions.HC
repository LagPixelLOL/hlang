// try/catch/throw: Fs->except_ch, catch_except rethrow search semantics
U0 D1(I64 n)
{
  "D1\n";
  if (n==1) throw('Pt1');
  if (n==2) throw('Pt2');
  if (n==3) throw;          //dft ch=0
}
U0 D2(I64 n)
{
  "D2\n";
  try {
    D1(n);
    "D1 returned\n";
  } catch {
    "D2 hndlr Ch:%c\n",Fs->except_ch;
    if (Fs->except_ch=='Pt1') {
      "Caught in D2\n";
      Fs->catch_except=TRUE;
    }
    //else: search continues to the outer handler
  }
}
U0 Demo(I64 n)
{
  try {
    D2(n);
    if (n==4) throw('Pt4');
  } catch {
    "Demo hndlr Ch:%c\n",Fs->except_ch;
    Fs->catch_except=TRUE;
  }
  "Demo done %d\n",n;
}
Demo(0);
Demo(1);
Demo(2);
Demo(3);
Demo(4);

// throw out of a loop, catch, state intact
I64 i,cnt=0;
try {
  for (i=0;i<100;i++) {
    cnt++;
    if (i==5) throw('loop');
  }
} catch {
  "caught %c after %d\n",Fs->except_ch,cnt;
  Fs->catch_except=TRUE;
}

// nested try in same function
try {
  try {
    throw('in');
  } catch {
    "inner %c\n",Fs->except_ch;
    //not caught -> rethrown
  }
} catch {
  "outer %c\n",Fs->except_ch;
  Fs->catch_except=TRUE;
}
"survived\n";

// PutExcept() as the handler: prints Except:'CH' and, with its default
// arg (catch_it=TRUE), claims the exception. The caller address printed
// is runtime-dependent, so pin except_callers[0] -- it is plain data.
try {
  throw('Psalm');
} catch {
  Fs->except_callers[0]=0;
  PutExcept;
}
"after PutExcept\n";
