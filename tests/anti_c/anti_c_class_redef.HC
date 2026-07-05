// ANTI-C: duplicate class definitions. C enforces the one-definition
// rule; the HolyC scoping table (scopinglinkage.md) marks `class` dups as
// ALLOWED -- "a dup overshadows the original" -- so a file can be
// re-#include'd while developing it. Earlier declarations keep the old
// layout; lookups after the dup see the new one.
class C { I64 x; };
C a;
a.x=42;
"%d %d\n",a.x,sizeof(C);         // 42 8

class C { I64 y,z; };            // overshadows, NOT an error
C b;
b.y=7; b.z=8;
"%d %d %d\n",b.y,b.z,sizeof(C);  // 7 8 16
"%d %d\n",offset(C.y),offset(C.z);

"%d\n",a.x;                      // 42 -- old instance keeps the old class

// functions and globals overshadow the same way (JIT table: DupsAllowed)
I64 F() { return 1; }
I64 F() { return 2; }
"%d\n",F;                        // 2
