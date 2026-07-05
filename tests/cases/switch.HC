// switch: null cases, ranges, default, unchecked [], fallthrough, nesting
I64 i;
for (i=0;i<20;i++)
  switch (i) {
    case: "Zero\n";   break; //Starts at zero
    case: "One\n";    break; //One plus prev case.
    case: "Two\n";    break;
    case: "Three\n";  break;
    case 10: "Ten\n"; break;
    case: "Eleven\n"; break;
  }

// ranges + null case following a range
for (i=0;i<10;i++)
  switch (i) {
    case 1...3: "A"; break;
    case: "B"; break;         //case 4 (one plus range end)
    case 7: "C"; break;
    default: "."; break;
  }
'\n';

// unchecked jump table form
for (i=0;i<5;i++)
  switch [i] {
    case 0: "w"; break;
    case 1...2: "x"; break;
    case 3: "y"; break;
    case 4: "z"; break;
  }
'\n';

// fallthrough
for (i=0;i<4;i++)
  switch (i) {
    case 0: "f0 ";
    case 1: "f1 ";  break;
    case 2: "f2 ";
    case 3: "f3 "; break;
  }
'\n';

// case with char consts and negative values, expressions
for (i=-1;i<3;i++)
  switch (i) {
    case -1: "neg "; break;
    case 'B'-'A': "one "; break;
    case 2*1: "two "; break;
    default: "dft "; break;
  }
'\n';

// nested switch
for (i=0;i<3;i++) {
  I64 j;
  for (j=0;j<2;j++)
    switch (i) {
      case 0:
        switch (j) {
          case 0: "a"; break;
          case 1: "b"; break;
        }
        break;
      case: "c"; break;
      default: "d"; break;
    }
}
'\n';
// break inside loop inside switch
switch (1) {
  case 1:
    for (i=0;i<10;i++) {
      if (i==2) break;
      "%d",i;
    }
    "|";
    break;
  case 2: "no"; break;
}
'\n';
