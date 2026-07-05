// pointers are just I64s: UNSCALED + - ++ --, scaled indexing
I64 arr[4];
arr[0]=100; arr[1]=200; arr[2]=300; arr[3]=400;
I64 *p=arr;         //array decays
"%d %d\n",*p,p[2];
p=p+8;              //advances ONE I64 = 8 BYTES (unscaled, like TempleOS)
"%d\n",*p;
p+=16;
"%d\n",*p;
p-=8;
"%d\n",*p;
// pointer difference is a byte count
I64 *p0=&arr[0],*p3=&arr[3];
"%d\n",p3-p0;       //24 bytes
// ++ moves one byte; use casts for typed stepping
U8 *b=arr(U8 *);
b++;
"%X\n",*b;          //second byte of 100 = 0
b--;
"%d\n",*b;          //100
// byte walk of an I64
U64 v=0x0102030405060708;
U8 *vb=&v(U8 *);
"%d %d %d\n",vb[0],vb[7],*vb;
// postfix casts
F64 f=3.9;
"%d\n",f(I64);           //C-style trunc toward zero
"%d\n",(-3.9)(I64);
I64 big=0x1FF;
"%d %d\n",big(U8),big(I8);   //255, -1
"%3.1f\n",big(F64)/2;
"%d\n",ToI64(3.9);
"%d %d %d\n",ToBool(5),ToBool(0),ToBool(-0.0);
"%3.1f\n",ToF64(7)/2;
// & and * round trips, char writes through pointers
I64 x=0x41;
I64 *px=&x;
*px=*px+1;
"%d\n",x;
U8 *s="hi";
"%c%c %d\n",s[0],s[1],s[2];
// pointer comparisons (unsigned) and NULL
"%d %d\n",px!=NULL,NULL==0;
// string pointers walk
U8 *w="walk";
I64 n=0;
while (*w) { n++; w++; }
"%d\n",n;
