// arrays: multi-dim, sizeof, decay, arrays in classes
I64 g[2][3];
I64 i,j;
for (i=0;i<2;i++)
  for (j=0;j<3;j++)
    g[i][j]=i*10+j;
"%d %d %d\n",g[0][0],g[1][2],g[0][2];
"%d %d %d\n",sizeof(g),sizeof(g[0]),sizeof(g[0][0]);
// array decays to pointer
I64 *p=g[1];
"%d\n",p[1];
// array member in class
class CRow
{
  I64 vals[4];
  U8 tag;
};
CRow r;
for (i=0;i<4;i++) r.vals[i]=i*i;
r.tag='R';
"%d %d %c %d\n",r.vals[2],r.vals[3],r.tag,sizeof(CRow);
// array of classes
class CP { I64 x,y; };
CP pts[3];
for (i=0;i<3;i++) {
  pts[i].x=i;
  pts[i].y=i*2;
}
"%d %d\n",pts[2].x,pts[2].y;
CP *pp=&pts[1];
"%d %d\n",pp->x,pp->y;
// local arrays with the compiler doing layout
U0 L()
{
  U8 grid[3][3];
  MemSet(grid,'.',9);
  grid[1][1]='X';
  I64 a,b;
  for (a=0;a<3;a++) {
    for (b=0;b<3;b++)
      '' grid[a][b];
    '\n';
  }
}
L;
// Option() accepted as a no-op (compiler options are meaningless here)
Option(OPTf_WARN_PAREN,ON);
"opt ok\n";
