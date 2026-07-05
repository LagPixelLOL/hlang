// #ifaot/#ifjit select on compilation mode (separate goldens per mode)
#ifjit
"mode:JIT\n";
#endif
#ifaot
"mode:AOT\n";
#endif
#ifjit
U8 *Mode() { return "jit"; }
#endif
#ifaot
U8 *Mode() { return "aot"; }
#endif
"fn says %s\n",Mode;
// __CMD_LINE__ follows the mode too: 1 when JIT'ed "from the cmd line",
// 0 in an AOT module (TempleOS: Fs->last_cc->flags&CCF_CMD_LINE)
"cmdline=%d\n",__CMD_LINE__;
"common\n";
