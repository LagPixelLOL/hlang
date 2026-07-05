// TempleOS operator precedence:
//   `  >>  <<   (tier 1)
//   *  /  %
//   &
//   ^
//   |
//   +  -
//   <  >  <=  >=  (chained!)
//   ==  !=
//   &&
//   ^^
//   ||
//   =  <<=  >>=  *=  /=  &=  |=  ^=  +=  -=
"%d\n",2`10;             //1024
"%d\n",2`3`2;            //left assoc: (2^3)^2 = 64
"%5.3f\n",2.0`0.5;       //sqrt 2
"%d\n",3*2`2;            //shift/pow tier binds tighter than *: 3*(2^2)=12
"%d\n",1<<4/2;           //(1<<4)/2 = 8  (<< is tier 1, / tier 2)
"%d\n",2+1&3;            //& tighter than +: 2+(1&3) = 3
"%d\n",4|1^1;            //^ tighter than |: 4|(1^1) = 4
"%d\n",8|4+1;            //| tighter than +: (8|4)+1 = 13
"%d\n",1+2*3-4;          //3
"%d\n",10-4-3;           //left assoc: 3
"%d\n",7&5^3|9;          //(((7&5)^3)|9) = 5^3=6 |9 = 15... check: 7&5=5,5^3=6,6|9=15
// chained comparisons
I64 age=15;
if (13<=age<20) "Teen-ager\n";
if (!(5<3<10)) "not chained like C\n";  //5<3 false -> whole chain false
I64 i=7,j=9;
"%d\n",5<i<j+1<20;       //1
"%d\n",5<i<j<8;          //0
// side effects evaluated once in chains
I64 n=0;
I64 Bump() { return ++n; }
"%d %d\n",1<=Bump<=1,n;  //1 1
// logical ops & short circuit
I64 hits=0;
I64 T() { hits++; return 1; }
I64 F() { hits++; return 0; }
"%d",F && T; "%d\n",hits;   //0 1
hits=0;
"%d",T || F; "%d\n",hits;   //1 1
"%d %d %d\n",1^^0,1^^1,0^^0;
// assignment ops (incl %=, and value is untruncated rhs)
I64 x=10;
x+=5;  "%d\n",x;
x-=3;  "%d\n",x;
x*=4;  "%d\n",x;
x/=6;  "%d\n",x;
x%=5;  "%d\n",x;
x<<=4; "%d\n",x;
x>>=2; "%d\n",x;
x|=9;  "%d\n",x;
x&=12; "%d\n",x;
x^=5;  "%d\n",x;
// ++/-- pre/post
I64 k=5;
"%d %d %d %d\n",k++,k,++k,k;
"%d %d\n",k--,--k;
// unary
"%d %d %d %d\n",-x,!x,!0,~0;
F64 f=-2.5;
"%3.1f %3.1f\n",-f,Abs(f);
