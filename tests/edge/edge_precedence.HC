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
"%d\n",-2`2;                 // (-2)`2 = 4  (unary is the pow operand)
"%d\n",-2+3;                 // 1

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

// postfix/prefix ordering within one expression (left-to-right eval)
I64 i=5;
I64 r=i++ + ++i;             // i++ ->5 (i=6); ++i ->7 (i=7); 5+7
"%d %d\n",r,i;               // 12 7

// compound-assign returns the new value
I64 z=10;
"%d\n",z+=5;                 // 15
"%d\n",z*=2;                 // 30

// evaluation order of call arguments is left-to-right
I64 seq=0;
I64 Step(I64 tag) { seq=seq*10+tag; return tag; }
"%d %d %d\n",Step(1),Step(2),Step(3);   // 1 2 3
"%d\n",seq;                              // 123

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
