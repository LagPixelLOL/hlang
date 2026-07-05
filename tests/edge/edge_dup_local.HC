//ERR: duplicate local variable 'x'
// HolyC local vars are NoDups (function-scope) -- unlike C there is no
// block-level shadowing of a local. This must be rejected.
U0 F()
{
  I64 x=1;
  {
    I64 x=2;
  }
  "%d\n",x;
}
F;
