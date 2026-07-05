// Mandelbrot.HC -- the obligatory fractal, in about a screenful.
//
//   $ hcc examples/Mandelbrot.HC
//
// Everything numeric in HolyC is I64 or F64 -- there is no F32, and all
// integer work happens in 64 bits no matter how small the variable
// looks. That makes numeric code refreshingly free of promotion traps:
// what you write is what the machine does.

#define WIDTH   78
#define HEIGHT  24
#define MAX_IT  32

// The palette is just bytes; shades[k] scales iteration count to ink.
// (A string STATEMENT would go to Print() -- but as an initializer it's
// only data, so the '%' inside is harmless.)
U8 *shades=" .,:;-=+*#%@";

U0 Mandelbrot(F64 x0,F64 x1,F64 y0,F64 y1)
{
  I64 row,col,it;
  F64 cr,ci,zr,zi,t;
  for (row=0;row<HEIGHT;row++) {
    for (col=0;col<WIDTH;col++) {
      // map the character cell onto the complex plane
      cr=x0+col*(x1-x0)/WIDTH;
      ci=y0+row*(y1-y0)/HEIGHT;
      // iterate z = z*z + c until it escapes the circle |z|=2
      zr=0; zi=0; it=0;
      while (it<MAX_IT && zr*zr+zi*zi<4.0) {
        t =zr*zr-zi*zi+cr;
        zi=2*zr*zi+ci;
        zr=t;
        it++;
      }
      // '' hands one computed character to PutChars(); 11*it/MAX_IT
      // is pure I64 math (truncating divide), indexing scales by the
      // element size, so shades[k] is exactly the byte you think.
      '' shades[11*it/MAX_IT];
    }
    '\n';
  }
}

// The whole set...
Mandelbrot(-2.2,1.0,-1.2,1.2);
// ...and a dive onto the seahorse valley, same code, tighter window.
"seahorse valley:\n";
Mandelbrot(-0.80,-0.70,0.05,0.15);
