//ERR: nested function definitions are not allowed
U0 Outer()
{
  U0 Inner() { "x\n"; }
}
Outer;