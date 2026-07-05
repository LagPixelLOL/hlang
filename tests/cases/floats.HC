// F64: arithmetic, conversions, defaults, math library
F64 a=1.5,b=2.25;
"%5.3f %5.3f %5.3f %8.5f\n",a+b,a*b,b-a,a/b;
"%d\n",a<b;
"%5.3f\n",a`2;          //power on floats
"%5.3f\n",2`-1.0;       //0.5 via F64 pow
// int<->float mixing promotes to F64
"%5.3f %5.3f\n",1/2.0,7%2.5;
"%d\n",3.99(I64);
// float compares chain too
F64 x=1.0;
if (0.5<x<1.5) "chained floats\n";
// math library
"%5.3f %5.3f %5.3f\n",Sqrt(2),Sin(0),Cos(0);
"%5.3f %5.3f\n",Ln(Exp(1)),Log10(1000.0);
"%5.3f %5.3f\n",Log2(8.0),Pow(2,10);
"%5.3f %5.3f %5.3f %5.3f\n",Floor(2.7),Ceil(2.1),Round(2.5),Trunc(-2.7);
"%5.3f %5.3f\n",ASin(Sin(0.5)),ATan(1)*4;
"%5.3f\n",Arg(1,1)*4;    //atan2(y=1,x=1)*4 = pi
"%d %d %d\n",MinI64(-3,4),MaxI64(-3,4),SignI64(-9);
"%d %d\n",AbsI64(-7),SqrI64(-5);
"%5.3f\n",Abs(-2.5);
// default F64 arg
F64 FD(F64 v=1.25) { return v*2; }
"%5.3f %5.3f\n",FD,FD(3.5);
// F64 globals & members
class CF { F64 r; I64 n; };
CF cf;
cf.r=0.5; cf.n=4;
"%5.3f\n",cf.r*cf.n;
// pi builtin const from prelude
"%8.6f\n",pi;
"%5.3f\n",Sin(pi/2);
