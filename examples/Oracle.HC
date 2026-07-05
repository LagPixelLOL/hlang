// Oracle.HC -- exceptions, HolyC style. Not C++: throw() is a FUNCTION
// taking an up-to-8-byte char constant, the handler reads Fs->except_ch,
// and -- the crucial twist -- a catch{} block does NOT end the search.
// The exception keeps unwinding OUTWARD unless the handler claims it by
// setting Fs->catch_except=TRUE.
//
//   $ hcc examples/Oracle.HC

U0 InnerSanctum(I64 omen)
{
  "  entering the inner sanctum (omen %d)\n",omen;
  if (omen==1) throw('Flood');    // 8-byte char consts are the payload
  if (omen==2) throw('Locusts');
  if (omen==3) throw;             // bare throw: char code 0
  "  the sanctum is quiet.\n";
}

U0 Priest(I64 omen)
{
  try {
    InnerSanctum(omen);
  } catch {
    // %c prints PACKED characters, so the code spells itself out.
    "  priest sees '%c'\n",Fs->except_ch;
    if (Fs->except_ch=='Flood') {
      "  priest: a flood I can handle. Bailing water.\n";
      Fs->catch_except=TRUE;      // claim it -- the search STOPS here
    }
    // anything else: catch_except stays FALSE and the very same
    // exception continues its journey to the caller's handler. This is
    // the documented TempleOS semantic, not a rethrow idiom.
  }
}

U0 Prophet(I64 omen)
{
  "prophet ponders omen %d\n",omen;
  try {
    Priest(omen);
  } catch {
    "  prophet inherits '%c' -- declares a fast.\n",Fs->except_ch;
    Fs->catch_except=TRUE;
  }
  "prophet is done with omen %d\n\n",omen;
}

Prophet(0);   // nothing thrown
Prophet(1);   // caught and claimed by the priest
Prophet(2);   // priest looks, declines, prophet inherits it
Prophet(3);   // bare throw: code 0, prophet gets it too

// A handler can also just be PutExcept() -- prints and claims. And note
// try/catch nests freely with loops; throwing OUT of a loop is fine.
I64 trial;
try {
  for (trial=1;;trial++) {
    "trial %d\n",trial;
    if (trial==3) throw('Enough');
  }
} catch {
  "verdict after %d trials: '%c'\n",trial,Fs->except_ch;
  Fs->catch_except=TRUE;
}
