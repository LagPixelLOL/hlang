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
"common\n";
