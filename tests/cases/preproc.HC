// preprocessor: #define/#undef/#if/#ifdef/#ifndef/#else/defined()/#assert
#define CNT     0x10
#define NAME    "Temple"
#define DOUBLE_CNT (CNT*2)
"%d %s %d\n",CNT,NAME,DOUBLE_CNT;
#assert CNT==16
#ifdef CNT
"defined\n";
#else
totally bogus tokens $$$ ((
#endif
#ifndef NOPE
"ndef ok\n";
#endif
#if CNT>10 && defined(NAME) || 0
"if ok\n";
#endif
#if 0
skipped { unbalanced " ignored
#if 1
nested skipped
#endif
#else
"else ok\n";
#endif
#undef CNT
#ifdef CNT
bogus
#else
"undef ok\n";
#endif
#define CNT 3
"%d\n",CNT;
// macros expand recursively, in code
#define TWICE DOUBLE
#define DOUBLE 2*
"%d\n",TWICE CNT+1;   // 2*3+1
// __LINE__ / __FILE__-ish builtins
#define AT __LINE__
"line=%d\n",AT;
"file has HC=%d\n",StrFind(".HC",__FILE__)!=NULL;
// #include
#include "preproc_inc.HH"
"%d %s\n",INC_CONST,IncFun;
