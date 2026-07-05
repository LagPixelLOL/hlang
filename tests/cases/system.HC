// time/system/file runtime: tS, Now, Sleep, FileRead, FileWrite.
// Wall-clock values change every run, so assert PROPERTIES, not values.

// tS = seconds since program start, F64
F64 t1=tS;
"%d\n",t1>=0;
Sleep(2);                        //a 2 mS nap
F64 t2=tS;
"%d %d\n",t2>=t1,t2-t1<60;       //time advanced, and by less than a minute

// Now = CDate: days since 1/1/0 in the high 32 bits, day-fraction below.
// 739k..800k covers 2024..2189 -- sane without being brittle.
I64 cd=Now;
I64 days=cd>>32;
"%d\n",739000<days<800000;       //chained comparison, of course
"%d\n",Now>=cd;                  //monotone

// FileWrite/FileRead round trip. Golden tests run 4x in parallel
// (JIT/AOT x -O0/-O2), so the scratch name must be unique per process:
// Seed() with no arg seeds from the clock.
Seed;
U8 *fname=MStrPrint("/tmp/hcc_sys_%X_%X.tmp",RandU64,Now);
U8 *msg="written by system.HC\n";
"%d\n",FileWrite(fname,msg,StrLen(msg));   //bytes written
I64 sz;
U8 *back=FileRead(fname,&sz);              //MAlloc'ed, NUL-terminated
"%d %d\n",sz,MemCmp(back,msg,sz);
"" back;                                   //round-tripped bytes, verbatim
Free(back);

// overwrite with binary bytes (embedded NUL) and read them back
U8 raw[4];
raw[0]='O'; raw[1]=0; raw[2]='K'; raw[3]='\n';
"%d\n",FileWrite(fname,raw,4);
back=FileRead(fname,&sz);
"%d %d%d%d%d\n",sz,back[0]=='O',back[1]==0,back[2]=='K',back[3]=='\n';
Free(back);
Free(fname);

// missing file: NULL buffer, size 0 (and Free(NULL) stays legal)
sz=123;
U8 *none=FileRead("/nonexistent/hcc_nope.tmp",&sz);
"%d %d\n",none==NULL,sz;
Free(none);
