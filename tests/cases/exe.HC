// #exe{} executes HolyC at compile time; StreamPrint injects source text
#exe {
  I64 i;
  StreamPrint("I64 gen_sum=%d;",1+2+3);
  for (i=0;i<4;i++)
    StreamPrint("\"i%d \";",i);
  StreamPrint("\"\\n\";");
}
"gen_sum=%d\n",gen_sum;
#exe {StreamPrint("#define GEN_D 99");}
"%d\n",GEN_D;
"done\n";
