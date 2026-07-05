// Dungeon.HC -- a tiny deterministic dungeon crawl that happens to be a
// guided tour of HolyC classes.
//
//   $ hcc examples/Dungeon.HC

// ------------------------------------------------------------ the cast
// 'class' is the only aggregate (no typedef, no struct keyword). Members
// are laid out PACKED, in order, with no C-style alignment padding --
// TempleOS pads by hand with members literally named 'pad' when it
// matters. It doesn't matter here, but sizeof() below shows the sums.
class CBeast
{
  CBeast *next;                 // self-reference is fine mid-definition
  U8 *name;
  I64 hp,atk;
  U0 (*war_cry)(CBeast *b);     // function-pointer member (see below)
};

// Single inheritance only ("Only one base class is allowed").
// A CDragon is a CBeast plus a hoard; the base's members come first.
class CDragon:CBeast
{
  I64 hoard;
};

"sizeof CBeast=%d CDragon=%d (packed sums, no padding)\n",
    sizeof(CBeast),sizeof(CDragon);

// ------------------------------------------------------------ war cries
U0 Growl(CBeast *b)
{
  "%s growls.\n",b->name;
}
U0 Shriek(CBeast *b)
{
  "%s shrieks!\n",b->name;
}
U0 Inferno(CBeast *b)
{
  // The dragon knows what it is; cast the base pointer back up.
  // Casts are POSTFIX in HolyC: expr(Type *) instead of (Type*)expr.
  CDragon *d=b(CDragon *);
  "%s breathes fire upon %d gold coins!\n",d->name,d->hoard;
}

// ------------------------------------------------------------ the horde
// Build a linked list on the heap. MAlloc/Free are the real TempleOS
// names (and Free(NULL) is legal, Terry said so).
CBeast *horde=NULL;

CBeast *Spawn(U8 *name,I64 hp,I64 atk,U0 (*cry)(CBeast *b))
{
  CBeast *b=MAlloc(sizeof(CBeast));
  b->name=name;
  b->hp=hp;
  b->atk=atk;
  b->war_cry=cry;
  b->next=horde;                // push on front
  horde=b;
  return b;
}

Spawn("Goblin",8,2,&Growl);     // &Fun takes a function's address
Spawn("Harpy",12,3,&Shriek);
CDragon *smaug=MAlloc(sizeof(CDragon));
smaug->name="Dragon";
smaug->hp=40; smaug->atk=9;
smaug->war_cry=&Inferno;
smaug->hoard=1000000;
smaug->next=horde;
horde=smaug;                    // a CDragon IS a CBeast (prefix layout)

// ------------------------------------------------------------ the hero
// Default args can sit ANYWHERE in the list, and callers may skip slots:
// Attack(,5) uses the default weapon but forces the roll. Defaults are
// re-evaluated at each call site -- they are expressions, not constants.
I64 Attack(U8 *weapon="sword",I64 roll)
{
  I64 dmg=roll+StrLen(weapon);  // longer weapon, better reach, obviously
  "  hero swings the %s for %d damage\n",weapon,dmg;
  return dmg;
}

// Seed the TempleOS RNG so every run of this example tells the same
// legend. RandU16 is called WITHOUT parentheses -- functions with no
// (required) args don't need them; 'RandU16;' is already a call.
Seed(42);

CBeast *b=horde;
while (b) {                     // classic list walk; pointers are I64s
  "A wild %s (hp %d) appears!\n",b->name,b->hp;
  b->war_cry(b);                // dispatch through the member fn ptr
  while (b->hp>0) {
    if (RandU16&1)
      b->hp-=Attack(,RandU16%6+1);        // skipped arg -> dft "sword"
    else
      b->hp-=Attack("halberd",RandU16%6+1);
  }
  "The %s is vanquished.\n\n",b->name;
  b=b->next;
}

// ------------------------------------------------------------ epilogue
// A sub_switch victory lap: start:/end: bracket a GROUP of cases with a
// shared front/back porch -- the porch runs for every case in the group.
// (This is real HolyC, from the docs' SubSwitch.HC.)
I64 day;
for (day=1;day<=6;day++)
  switch (day) {
    case 1: "Mon: sharpen sword. ";  break;
    start:
      "[feast day: ";
      case 2: "roast goblin";  break;
      case 3: "harpy soup";    break;
      case 4: "dragon steak";  break;
    end:
      "] ";
      break;
    case 5: "Fri: count gold. "; break;
    default: "Rest. ";           break;
  }
'\n';

// Tidy up the corpses (list nodes); the dragon was on the same list.
b=horde;
while (b) {
  CBeast *nxt=b->next;
  Free(b);
  b=nxt;
}
