// sub_switch start:/end: groups (SubSwitch.HC)
U0 SubSwitch()
{
  I64 i;
  for (i=0;i<10;i++)
    switch (i) {
      case 0: "Zero ";  break;
      case 2: "Two ";   break;
      case 4: "Four ";  break;
      start:
        "[";
        case 1: "One";  break;
        case 3: "Three";break;
        case 5: "Five"; break;
      end:
        "] ";
        break;
    }
  '\n';
}
SubSwitch;

// porch runs for every grouped case; breaks route through the end porch
I64 i;
for (i=0;i<7;i++)
  switch (i) {
    case 0: "z"; break;
    start:
      "<%d:",i;
      case 1: "one"; break;
      case 2: "two";        //falls through to case 3's body
      case 3: "three"; break;
      case 4...5: "45"; break;
    end:
      ">";
      break;
    case 6: "six"; break;
  }
'\n';
