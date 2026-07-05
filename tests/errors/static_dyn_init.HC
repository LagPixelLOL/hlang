//ERR: static var initializer must be a compile-time constant
U0 F()
{
  static I64 x=RandU64;
  "%d\n",x;
}
F;
