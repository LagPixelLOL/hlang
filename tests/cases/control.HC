// control flow: loops, goto (no continue in HolyC!), labels, nesting
I64 i=0,sum=0;
loop:
  i++;
  if (i&1) goto loop;   //odd -> skip (this is the HolyC 'continue')
  sum+=i;
  if (i<10) goto loop;
"%d\n",sum;

// while / do-while
i=0;
while (i<3) { "%d",i; i++; }
'\n';
i=0;
do { "%d",i; i++; } while (i<3);
'\n';
i=99;
while (i<3) "never\n";
do "once\n"; while (0);

// for with multiple init/post expressions, empty sections
I64 j,k;
for (j=0,k=10;j<k;j++,k--) ;
"%d %d\n",j,k;
for (i=0;;i++) {
  if (i==4) break;
}
"%d\n",i;
i=0;
for (;i<3;) i++;
"%d\n",i;

// nested loops + goto out
for (i=0;i<5;i++)
  for (j=0;j<5;j++) {
    if (i*j==6) goto out;
  }
out:
"%d %d\n",i,j;

// forward goto
goto fwd;
"skipped\n";
fwd:
"fwd ok\n";

// empty statements & blocks
;;;
{ }
if (1) ; else "no\n";
"end\n";
