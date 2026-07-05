// mandel -- Mandelbrot escape-iteration sum: F64 multiply/add/compare in
// a data-dependent loop. Plain IEEE ops only, so the total is exact and
// identical in both languages.
#define W     1600
#define H     800
#define MAXIT 256

I64 x,y,it,total=0;
F64 cr,ci,zr,zi,t;
for (y=0;y<H;y++)
  for (x=0;x<W;x++) {
    cr=-2.5+x*3.5/W;
    ci=-1.25+y*2.5/H;
    zr=0; zi=0; it=0;
    while (it<MAXIT && zr*zr+zi*zi<4.0) {
      t=zr*zr-zi*zi+cr;
      zi=2*zr*zi+ci;
      zr=t;
      it++;
    }
    total+=it;
  }
"%d\n",total;
