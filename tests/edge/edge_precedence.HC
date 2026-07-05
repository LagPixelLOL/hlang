// EDGE: precedence corners, unary chains, chained & nested assignment,
// evaluation ordering.

// deep unary chains
"%d\n",- - -5;               // -5
"%d\n",!!5;                  // 1
"%d\n",!!!5;                 // 0
"%d\n",~~5;                  // 5
"%d\n",-~5;                  // ~5=-6, -(-6)=6
"%d\n",~-5;                  // -(5)=-5, ~(-5)=4
"%d\n",!-5;                  // -5 truthy -> 0

// unary binds tighter than binary
"%d\n",!5<3;                 // (!5)<3 = 0<3 = 1
"%d\n",-2+3;                 // 1
// NOTE: unary-vs-backtick precedence is unspecified by the HolyC docs, so
// we do NOT assert a value for -2`2 here (would be projecting an
// assumption). Parenthesize when you mean it: (-2)`2 vs -(2`2).
"%d %d\n",(-2)`2,-(2`2);     // 4 -4  (explicit)

// nested parens
"%d\n",((((1+2))))*3;        // 9
"%d\n",2*(3+(4*(5-1)));      // 2*(3+16)=38

// chained assignment (right-assoc), all take the value
I64 a,b,c;
a=b=c=7;
"%d %d %d\n",a,b,c;          // 7 7 7

// assignment is an expression yielding the (untruncated) RHS
I64 x,y;
x=(y=3)+1;
"%d %d\n",x,y;               // 4 3
"%d\n",(a=10);               // 10

// per-target truncation but expression yields untruncated value (doc rule)
I8 n8;
I64 wide;
wide=n8=0x1FF;
"%d %d\n",n8,wide;           // -1 511

// ++ / -- semantics, each in its OWN statement. (We deliberately avoid
// expressions like i++ + ++i: modifying a var twice in one expression is
// unspecified in C and the HolyC docs give no ordering guarantee -- testing
// a specific result there would be projecting C's (non-)rules onto HolyC.)
I64 i=5;
"%d ",i++; "%d\n",i;         // 5 6
"%d ",++i; "%d\n",i;         // 7 7
"%d ",i--; "%d\n",i;         // 7 6
"%d ",--i; "%d\n",i;         // 5 5

// compound-assign returns the new value
I64 z=10;
"%d\n",z+=5;                 // 15
"%d\n",z*=2;                 // 30

// hcc evaluates call arguments left-to-right. NOTE: argument evaluation
// order is unspecified in C and unstated in the HolyC docs; this locks in
// hcc's (deterministic, JIT==AOT) behavior, it is NOT a HolyC guarantee.
I64 seq=0;
I64 Step(I64 tag) { seq=seq*10+tag; return tag; }
"%d %d %d\n",Step(1),Step(2),Step(3);   // hcc: 1 2 3
"%d\n",seq;                              // hcc: 123

// arithmetic mixed into a comparison chain
"%d\n",1+1<3<5;              // (2<3)&&(3<5) = 1
"%d\n",5>4>0;               // (5>4)=1, (1>0)... chain: 5>4 && 4>0 = 1

// precedence: = is lowest, so this parses a=(b=(2<3))
a=b=2<3;
"%d %d\n",a,b;               // 1 1

// comma operator context: for-init/post lists
I64 p,q;
for (p=0,q=9; p<q; p++,q--) ;
"%d %d\n",p,q;               // p and q cross near the middle
