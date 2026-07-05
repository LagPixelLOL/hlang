//ERR: no 'continue' stmt in HolyC
I64 i;
for (i=0;i<10;i++) {
  if (i&1) continue;
  "%d",i;
}
