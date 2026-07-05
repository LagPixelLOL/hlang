// globals: const init, dynamic init in order, statics, initializer lists
I64 g1=5;
I64 g2=g1*10;            //dynamic init, runs in program order
"%d %d\n",g1,g2;
g1=7;
I64 g3=g1+g2;
"%d\n",g3;
U8 gs[8]="abc";
"%s %d\n",gs,sizeof(gs);
I64 tbl[4]={1,2,3,4};
"%d %d\n",tbl[0]+tbl[3],tbl[1]*tbl[2];
I64 part[5]={9,8};
"%d %d %d\n",part[0],part[1],part[4];
F64 fg=2.5;
"%3.1f\n",fg;
class CPos { I64 x,y; };
CPos gp={11,22};
"%d %d\n",gp.x,gp.y;
// local versions
U0 Locals()
{
  I64 t[3]={5,6,7};
  U8 s[6]="hey";
  CPos p={1,2};
  "%d %s %d %d\n",t[0]+t[1]+t[2],s,p.x,p.y;
  static I64 counter=100;
  counter++;
  "%d\n",counter;
}
Locals;
Locals;
Locals;
// multiple declarators, mixed pointers ("U8 *a,b": a is ptr, b is U8)
U8 *pa,pb;
pa="x";
pb=0x141;
"%s %d\n",pa,pb;
// globals in functions before their textual init still exist (zeroed)
I64 late;
U0 UseLate() { "%d\n",late; }
UseLate;
late=123;
UseLate;
