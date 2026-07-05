// EDGE: switch corners -- ranges, negative cases, null-case counting,
// default placement, fallthrough chains, nested sub_switch, single case,
// default-only, empty body.

I64 i;

// negative case values + default
for (i=-3;i<=3;i++)
  switch (i) {
    case -3: "a"; break;
    case -1: "b"; break;
    case 0:  "c"; break;
    case 2:  "d"; break;
    default: "."; break;
  }
'\n';                        // a.b c.d.

// wide-ish range + range then null case (null = one past range end)
for (i=0;i<12;i++)
  switch (i) {
    case 2...5: "R"; break;
    case: "N"; break;         // case 6
    case 9...10: "S"; break;
    default: "."; break;
  }
'\n';                        // ..RRRRN..SS.

// long fallthrough chain
for (i=0;i<6;i++) {
  switch (i) {
    case 0: "0";
    case 1: "1";
    case 2: "2"; break;
    case 3: "3";
    case 4: "4"; break;
    case 5: "5"; break;
  }
  "|";
}
'\n';                        // 012|12|2|34|4|5|

// single case only, and default-only
switch (5) { case 5: "hit\n"; break; }
switch (99) { default: "dft\n"; break; }
switch (7) { case 1: "no\n"; break; }   // no match, no default -> nothing
"after\n";

// null cases counting from zero, interrupted by explicit, then continuing
for (i=0;i<7;i++)
  switch (i) {
    case: "z";  break;        // 0
    case: "o";  break;        // 1
    case 4: "F"; break;
    case: "f"; break;         // 5
    default: "."; break;
  }
'\n';                        // zo..F f... wait compute below

// nested sub_switch inside a normal switch
for (i=0;i<6;i++)
  switch (i) {
    case 0: "Z"; break;
    start:
      "[";
      case 1: "one"; break;
      case 2...3: "mid"; break;
      case 4: "four"; break;
    end:
      "]"; break;
    case 5: "V"; break;
  }
'\n';

// switch on an expression with side effects (evaluated once)
I64 calls=0;
I64 Pick() { calls++; return 2; }
switch (Pick) {
  case 1: "one"; break;
  case 2: "two"; break;
  default: "d"; break;
}
"\n%d\n",calls;              // two / 1

// switch expression using chained/arith
i=5;
switch (i*2) {
  case 10: "ten\n"; break;
  default: "no\n"; break;
}

// break inside nested loop inside a case only exits the loop
switch (1) {
  case 1:
    for (i=0;i<10;i++) {
      if (i==3) break;
      "%d",i;
    }
    "|end\n";
    break;
}

// default NOT last, and fallthrough out of default into a later case
for (i=0;i<4;i++) {
  switch (i) {
    default: "D";
    case 1: "1"; break;
    case 3: "3"; break;
  }
  "|";
}
'\n';                        // i=0:D1| i=1:1| i=2:D1| i=3:3|

// empty switch body (well-formed, matches nothing)
switch (3) { }
"empty ok\n";

