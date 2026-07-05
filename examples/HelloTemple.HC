// HelloTemple.HC -- a tour of HolyC's print sugar.
//
//   $ hcc examples/HelloTemple.HC              # JIT, the TempleOS way
//   $ hcc examples/HelloTemple.HC -o hello     # or AOT to a native binary
//
// There is no main() in HolyC. Everything at top level runs at startup,
// in order, like a script -- because in TempleOS the "shell" IS the
// compiler and every command line is JIT-compiled HolyC.

// A string all alone is a statement: it is sent straight to Print().
"Hail, terminal!\n";

// ...and Print() means format codes work without naming any function:
"The temple has %d pillars and %5.2f cubits of gold.\n",7,12.5;

// An empty string literal means "the NEXT expression is the fmt string".
// That's how you forward a format you got from somewhere else:
U8 *fmt="%s slept %d hours.\n";
"" fmt,"The offering",8;

// A char const all alone goes to PutChars(). Char consts pack up to
// EIGHT characters into one I64, little-endian -- 'Hi' == 0x6948.
// So this "string" is really just two integer statements:
'Hello ';
'World\n';

// '' sends a computed value to PutChars(). Codepoint arithmetic is
// plain I64 arithmetic ('A'+1 is 'B' -- there is no char type, only U8
// storage that widens to 64 bits the moment you touch it).
I64 ch;
for (ch='A';ch<='E';ch++)
  '' ch;
'\n';

// Adjacent string literals concatenate, handy for long lines:
"This sentence was assembled "
    "from three separate literals, "
    "and nobody can tell.\n";

// %z indexes into a \0-separated list -- a TempleOS favorite for enums:
I64 rank;
for (rank=0;rank<4;rank++)
  "Promotion to %z!\n",rank,"Cadet\0Ensign\0Captain\0Admiral";

// $ is the DolDoc escape character in TempleOS, so a literal dollar is
// written $$ (and %% is a literal percent, as in C):
"Tithe: $$10 (100%% voluntary).\n";
