// FizzBuzz.HC -- FizzBuzz three ways, each one leaning on something
// HolyC does differently from C.
//
//   $ hcc examples/FizzBuzz.HC

// ---------------------------------------------------------------- way 1
// The classic. HolyC has NO ternary operator and NO continue statement
// (the docs: "There is no continue stmt. Use goto.") -- so the idiomatic
// skip is a goto to the bottom of the loop body. This is not a war
// crime; it is the documented style.
"--- goto (there is no 'continue') ---\n";
I64 i;
for (i=1;i<=15;i++) {
  if (!(i%15)) { "FizzBuzz\n"; goto next; }
  if (!(i%3))  { "Fizz\n";     goto next; }
  if (!(i%5))  { "Buzz\n";     goto next; }
  "%d\n",i;
next:
}

// ---------------------------------------------------------------- way 2
// switch on i%15. HolyC switches always build a jump table, and case
// labels stack for fallthrough exactly as you'd hope. 15 possible
// remainders -> one table, zero comparisons.
"--- jump table ---\n";
for (i=1;i<=15;i++) {
  switch (i%15) {
    case 0:                        "FizzBuzz"; break;
    case 3: case 6: case 9: case 12: "Fizz";   break;
    case 5: case 10:                 "Buzz";   break;
    default: "%d",i;                            break;
  }
  '\n';
}

// ---------------------------------------------------------------- way 3
// Buzz as DATA: index a \0-list with %z and pick the string with
// arithmetic instead of branching. (!x is 0 or 1 -- multiply and add.)
//   sel = 0 none, 1 fizz, 2 buzz, 3 both
"--- branchless-ish, %%z as the lookup ---\n";
I64 sel;
for (i=1;i<=15;i++) {
  sel=!(i%3)+2*!(i%5);
  if (sel)
    "%z\n",sel-1,"Fizz\0Buzz\0FizzBuzz";
  else
    "%d\n",i;
}
