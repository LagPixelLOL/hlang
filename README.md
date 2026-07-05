# hlang — a HolyC compiler (`hcc`)

An LLVM front end for Terry A. Davis' **HolyC**, with both **JIT execution**
(the TempleOS way) and **AOT compilation** to native objects/executables,
plus a minimal stdlib in the spirit of the TempleOS kernel library.

The language implementation follows the archived HolyC documentation
(`holyc-docs`, an archive of `templeos.holyc.xyz/Wb/Doc/HolyC.html`); the
doc's example programs (`NullCase.HC`, `SubSwitch.HC`, `SubIntAccess.HC`,
`Exceptions.HC`, the hello-world variants) run unmodified.

```
$ cat hello.HC
"Hello World\n";

$ hcc hello.HC              # JIT: code outside functions runs at startup
Hello World
$ hcc hello.HC -o hello     # AOT: native executable
$ ./hello
Hello World
```

## Layout

| dir        | contents                                                    |
|------------|-------------------------------------------------------------|
| `src/`     | the compiler: lexer+preprocessor, parser, LLVM codegen, ORC JIT, AOT driver |
| `runtime/` | `hcrt` — the C runtime backing the stdlib (linked into `hcc` for JIT, `libhcrt.a` for AOT) |
| `lib/`     | `HolyC.HH` — the auto-included stdlib prelude (KernelA.HH spirit) |
| `tests/`   | golden tests (run under **both** JIT and AOT), error tests, front-end dumps |

## Build & test

Requires CMake, ninja/make, a C++17 compiler and LLVM 21 dev libraries
(`llvm-21-dev`, `libzstd-dev`, `zlib1g-dev` on Debian/Ubuntu).

```sh
cmake -S . -B build -G Ninja
ninja -C build
ninja -C build check        # run the test suite (58 tests)
```

`hcc` links LLVM/zstd/zlib/libstdc++ statically — the resulting binary
depends only on libc/libm and is deployable standalone next to its
`lib/` prelude directory and `libhcrt.a` (all placed in `build/`).
AOT executables depend only on libc/libm. Linking uses the system `cc`
(override with `HCC_CC=`).

## Usage

```
hcc file.HC [-- args...]     JIT compile and run (default)
hcc file.HC -o prog          AOT compile + link an executable
hcc file.HC -c file.o        AOT compile to an object file
  --emit-llvm                print LLVM IR
  --dump-tokens/--dump-ast   front-end debugging
  -O0..-O3                   optimization (default -O0)
  -I dir                     include search dir
  --no-prelude               don't auto-include lib/HolyC.HH
```

## Language support (per the spec)

* **Types** `U0 I0 I8 U8 I16 U16 I32 U32 I64 U64 F64` (+ the `*i`
  intrinsic forms), pointers, arrays, classes. No `F32`. No `typedef`
  (use `class`); `Bool`/`TRUE`/`FALSE`/`NULL`/`ON`/`OFF` are prelude
  `#define`s, as in TempleOS.
* **64-bit semantics**: "All values are extended to 64-bit when accessed.
  Intermediate calculations are done with 64-bit values." All four doc
  examples produce the documented results, including the reg-var
  behavior of 32-bit locals (`I32 i4=0x80000000; i4>>1 == 0x40000000`
  but `I32 i5=-0x80000000; i5>>1 == 0xFFFFFFFFC0000000`) and truncating
  assignment for 8/16-bit variables (`i1=0x12345678` → `0x5678`).
  `U64` operands select unsigned divide/mod/shift/compare.
* **TempleOS operator precedence**, exactly:
  `` ` ``,`>>`,`<<` | `*`,`/`,`%` | `&` | `^` | `|` | `+`,`-` |
  `<`,`>`,`<=`,`>=` | `==`,`!=` | `&&` | `^^` | `||` | assignments.
  So `2+1&3 == 3` and `1<<4/2 == 8`. The backtick operator raises to a
  power (integer or F64). There is **no** question-colon operator.
* **Chained comparisons**: `5<i<j+1<20` means `5<i && i<j+1 && j+1<20`
  with single evaluation and short circuit.
* **Assignment value is the untruncated RHS**: `j1=i1=0x12345678`
  leaves `i1==0x5678`, `j1==0x12345678`.
* **Postfix type casts**: `expr(U8 *)`, `MemberMetaData(...)(F64)`, plus
  the `ToI64()/ToF64()/ToBool()` intrinsics.
* **Print sugar**: `"str";` → `Print`; `"fmt %d\n",x;`;
  `"" fmt,args;` (empty string = variable fmt); `'c';` and `'' expr;` →
  `PutChars`; adjacent string literal concatenation; `'ABC'==0x434241`
  multi-char char consts (up to 8); `$$` = literal `$`.
* **No main()**: top-level statements execute at startup, in order
  (`__HC_startup`; AOT emits a `main` wrapper that calls it).
* **Functions**: default args anywhere (`Test(,3)` skips), calls without
  parentheses (`Dir;`, `return YorN;`), `&Fun` for addresses, function
  pointers (declared C-style) callable directly or via `Call()`,
  duplicate definitions overshadow earlier ones.
* **Varargs**: `...` with the `I64 argc`/`I64 argv[]` builtins; F64
  varargs occupy 64-bit slots (`"%f"` reinterprets); forwarding via
  `StrPrintJoin(NULL,fmt,argc,argv)` works as in TempleOS.
* **switch**: jump tables, `case 4...7:` ranges, null cases (`case:` =
  prev+1, starting at 0), unchecked `switch [i]`, `default:`,
  fallthrough, and **sub_switch** `start:`/`end:` groups with correct
  porch semantics (`break` inside a group routes through the end porch).
* **Classes**: single inheritance (`class B:A`), `union`s, typed unions
  (`public I64i union I64 {...}` — type in front is the whole-access
  type), sub-int access (`q.i32[1].u8[2]`, `i.u16[1]=0x9ABC`), member
  arrays, multiple `pad`/`reserved` members, member meta data parsed,
  instances declared after the class body, `sizeof`/`offset(cls.member)`
  (one level, per spec), `lastclass` default args resolved to the
  previous argument's class name at each call site.
* **try/catch/throw**: `throw('Ch8')` is a function with an ≤8-byte char
  arg; `Fs->except_ch` readable in `catch{}`; the handler search
  *continues outward* unless `Fs->catch_except=TRUE` (exact TempleOS
  semantics); `PutExcept()`, `Fs->except_callers[0]`. Implemented with
  setjmp frames in hcrt.
* **Preprocessor** (inside `Lex()`, no separate pass): `#include ""`
  (no `<>` form, as the spec demands), object-like `#define` (function
  macros rejected: "I'm not a fan"), `#undef`, `#if`/`#ifdef`/
  `#ifndef`/`#else`/`#endif` with `defined()`, `#ifaot`/`#ifjit`
  (selects on compilation mode), `#assert` (warns), `#help_index`/
  `#help_file` ignored, and **`#exe {...}`** — the block is compiled and
  run at compile time by the in-process JIT, and its `StreamPrint()`
  output is spliced into the token stream. `__DATE__ __TIME__ __FILE__
  __LINE__ __DIR__ __CMD_LINE__` are builtins.
* **Misc**: `goto`/labels (there is **no** `continue` — hcc says "use
  goto" like the doc does), `no_warn`, `reg`/`noreg` accepted (register
  allocation is LLVM's job), `public`/`static`/`extern`/`import`/
  `_extern SYM`/`_import SYM` linkage (in a hosted world `import`
  behaves like `extern`), `interrupt`/`haserrcode`/`argpop`/`noargpop`
  parsed, `lock {}` compiles its body (see limitations), `Option()`/
  `OPTf_*` accepted as no-ops.

### Pointer arithmetic

Pointers are just I64s ("all values are extended to 64-bit"): `+ - += -=
++ --` on pointers move by **bytes**, not elements. Indexing `p[i]`
*does* scale by the element size. Cast to step: `q=_d+ml->offset;` and
`b=arr(U8 *); b++;` are the idiomatic forms, exactly like TempleOS code.

## The stdlib (`lib/HolyC.HH` + `runtime/hcrt.c`)

Minimal but faithful to TempleOS names and behaviors:

* **Output**: `Print` (TempleOS fmt codes: `%d %u %x %X %c` (packed
  chars) `%s %q/%Q %f %e %g %p/%P %z` (indexed `\0`-list) `%D %T`
  (CDate), flags `-`/`0`/`,`(grouping)/`+`, width/precision),
  `PutChars`, `PutChar`, `PutS`, `StrPrint`, `MStrPrint`, `CatPrint`,
  `StrPrintJoin`.
* **Input**: `GetStr` (MAlloc'ed line), `GetChar`, `GetI64(msg,dft,min,
  max)`, `GetF64`, `YorN`, `PressAKey`.
* **Memory**: `MAlloc`, `CAlloc`, `MAllocIdent`, `Free` (NULL ok!),
  `MSize`, `MemCpy`, `MemSet`, `MemCmp`.
* **Strings**: `StrLen StrCpy StrNew StrCmp StrNCmp StrICmp StrFind
  Str2I64 Str2F64` (+ HolyC-implemented `ToUpper ToLower StrOcc SwapI64
  ClampI64`).
* **Math**: `Abs Sqrt Sin Cos Tan ASin ACos ATan Arg Ln Log2 Log10 Exp
  Pow Floor Ceil Round Trunc AbsI64 MinI64 MaxI64 SignI64 SqrI64`, and
  `pi`/`log2_10`/`log10_2`/`loge_2` consts.
* **Random/bits**: `Seed RandU64 RandI64 RandU32 RandU16`,
  `Bt Bts Btr Btc BCnt`.
* **Time/system**: `tS` (seconds), `Now` (CDate, days since 1/1/0),
  `Sleep(mS)`, `Exit`, `Call(addr)`, `FileRead`/`FileWrite`
  (MAlloc'ed buffers), and hosted extras `ArgCnt`/`ArgStr`.
* **Exceptions**: `throw`, `PutExcept`, `Fs` (a `CTask*` with
  `except_ch`, `catch_except`, `except_callers`, `task_name`).

Runtime symbols live in `hcrt` under `HC_*` names; the prelude binds
them to their HolyC names with `_extern`, exactly how TempleOS binds C
to asm.

## Documented limitations

Things TempleOS can do that a hosted, portable compiler reasonably
cannot (or that are deliberately out of scope):

* **`asm {}` blocks are rejected** with a clear error. Writing an
  x86-64 assembler with TempleOS' nonstandard opcodes/`DU8`/`ORG`/
  `BINFILE` machinery is out of scope; LLVM owns code generation here.
  Consequently `reg R15`-style register pinning is also ignored.
* **`$` (instruction pointer / class-offset hacks)** in expressions and
  class bodies is rejected (`Directives.HC`'s `"%08X",$` demo). `$` as
  the DolDoc escape in strings *is* handled (`$$` → `$`).
* **Compiler reflection** (`CHashClass`/`CMemberLst`/`MemberMetaData`,
  `ClassRep`, `DocForm`) is not exposed: member meta data is parsed and
  discarded. `Fs` carries only the exception-related fields, not the
  full TempleOS task struct; there is no `Fs->hash_table`.
* **`lock {}`** compiles its body without LOCK prefixes (the hosted
  runtime is single-threaded; `JobQue`/`Spawn` don't exist). Same for
  `CallStkGrow` (host stacks grow), `HeapCtrlInit`, graphics
  (`GrPrint`, `CDC`), `Adam`, and the rest of the OS surface — out of
  scope for a *minimal* stdlib.
* **Task/scope model**: TempleOS JIT scoping uses per-task hash tables
  with parent inheritance. Hosted `hcc` compiles one program per
  invocation; `extern`/`import` resolve against the process/link
  instead. Duplicate definitions overshadow, as documented.
* **`switch` is lowered through LLVM's switch** (which builds jump
  tables); `switch []` therefore still bound-checks — it's the same
  speed or better, never wrong, just not literally unchecked.
* **`#exe {}`** runs with the hcc runtime, not inside a TempleOS task;
  `Fs->last_cc`-based tricks won't work (the `__DATE__`-style directives
  are builtins instead). `StreamDir`/`Cd` are not provided.
* **Static local vars** require compile-time-constant initializers
  (TempleOS: "StaticVarInit" only, same restriction).
* **Returning out of a `try{}`** skips the frame pop (TempleOS likewise
  warns "don't return out of a catch"); use normal control flow.
* 10-byte (80-bit) float consts for `pi` etc. are 64-bit here
  (`OPTf_NO_BUILTIN_CONST` territory); `F64` is IEEE double either way.
* `%c` repeat counts (`%3c`) and DolDoc-specific fmt codes are not
  implemented; `%P` prints hex (no symbol table lookup at runtime).

## Testing

`tests/run_tests.sh` (also `ninja -C build check`) runs:

* **golden tests** in `tests/cases/` — each `.HC` runs under **JIT and
  AOT** and must byte-match its `.out` (per-mode `.jit.out`/`.aot.out`
  for `#ifjit`/`#ifaot`); stdin fixtures via `.in`;
* **error tests** in `tests/errors/` — must fail with the message named
  in their `//ERR:` header;
* **front-end goldens** in `tests/frontend/` — `--dump-tokens` /
  `--dump-ast` output.

The cases include the doc's examples verbatim (widening, `NullCase`,
`SubSwitch`, `SubIntAccess`, hello-world variants) plus systematic
coverage of precedence, chaining, defaults/skipped args, varargs,
classes, exceptions, sub-int access, pointers, the preprocessor, `#exe`,
strings/memory/math, and I/O.
