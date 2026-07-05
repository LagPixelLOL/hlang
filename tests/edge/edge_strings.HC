// EDGE: string escapes, embedded nulls, 8-char consts, format-string edges.

// every escape hcc supports (\n \t \r \0 \\ \" \x are doc-grade;
// \a \b \f \v are a hosted superset -- not asserted as canonical HolyC)
"esc:[\t][\n][\r ->][\a][\b][\f][\v][\\][\"][\']\n";
"hex:[\x41\x42\x43][\x7A]\n";        // ABC z

// an embedded \0 terminates a format string (it's a char*), faithful to C.
// To carry real embedded nulls, use a sized buffer + byte access:
U8 nb[8];
MemCpy(nb,"ab\0cd\0e",8);
"embedded:%d%d%d%d%d\n",nb[0],nb[2],nb[3],nb[5],nb[6];  // 97 0 99 0 101

// $$ is a literal $ (DolDoc escape); single $ passes through
"money:$$5 and $$$$\n";              // $5 and $$

// 8-char packed char consts (max), little-endian packing
"%X\n",'A';                          // 41
"%X\n",'AB';                         // 4241
"%X\n",'12345678';                  // full 8 bytes packed
PutChars('Hello ');
PutChars('World!\n');
'' 'Hi'+0; '\n';                     // '' sends the value to PutChars

// empty char const '' is 0
"%d\n",'';                           // 0

// adjacent string literal concatenation across lines
U8 *msg="line1 "
        "line2 "
        "line3";
"%s\n",msg;

// format width / precision / flags
"[%5d][%-5d][%05d][%+d]\n",42,42,42,42;
"[%8.3f][%-8.3f][%+.2f]\n",3.14159,3.14159,3.14159;
"[%10s][%-10s][%.3s]\n","hi","hi","truncated";
"[%,d][%,d]\n",1234567,-1234567;
"[%x][%X][%08X]\n",255,255,255;
"[%c][%c]\n",'!','AB';               // packed chars in %c
"literal %% percent\n";
"%d %d %d\n",1,2,3;

// %s with NULL prints (null); %P prints hex pointer
"[%s]\n",NULL(U8 *);
"%P\n",0xDEADBEEF;

// %z indexed \0-list
U8 *ranks="Cadet\0Ensign\0Captain\0Admiral";
"%z %z %z\n",0,ranks,2,ranks,3,ranks;

// out-of-range %z / unknown code passes through
"unknown:[%w]\n",5;                  // %w not a code -> literal-ish

// string helpers on edge inputs
"%d %d\n",StrLen(""),StrLen("abc");
"%d\n",StrCmp("","");               // 0
"%d\n",StrFind("x","")==NULL;       // 1 (not found in empty)
U8 buf[4]="ab";                      // fits with room
"%s %d\n",buf,StrLen(buf);
U8 exact[3]="abc";                   // fills, StrPrint would need the null; here init copies "abc\0" truncated to 3 -> no null!
// (declared size 3 holds 'a','b','c'; no terminator -- HolyC lets you)
"%c%c%c\n",exact[0],exact[1],exact[2];
