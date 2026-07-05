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
| `lib/`     | `HolyC.HH` — the auto-included stdlib prelude (KernelA.HH spirit) + the stdlib reference doc |
| `examples/`| fun, heavily-commented HolyC programs (fractals, dungeons, `#exe{}` magic) |
| `checklist/`| per-claim conformance audit against the HolyC docs (implemented / tested / deviations) |
| `tests/`   | golden tests (run under **JIT and AOT, at -O0 and -O2**), error tests, front-end dumps |

## Build & test

Requires CMake, ninja/make, a C++17 compiler and LLVM 21 dev libraries
(`llvm-21-dev`, `libzstd-dev`, `zlib1g-dev` on Debian/Ubuntu).

```sh
cmake -S . -B build -G Ninja
ninja -C build
ninja -C build check        # run the test suite
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

## Language (per the spec)

Everything the archived doc specifies is implemented: the type set (no
`F32`, no `typedef`), print sugar, default/skipped args and no-paren
calls, `argc`/`argv` varargs, chained comparisons, postfix casts,
classes/unions/sub-int access, `switch` with ranges/null cases and
`sub_switch` porches, `try`/`catch`/`throw` with the outward handler
search, and the `Lex()`-integrated preprocessor with `#exe {}`. Every
"there is no X" clause (`continue`, `typedef`, `?:`, `#define`
functions, `#include <>`) is enforced with a diagnostic that quotes the
doc.

**The full conformance record — each doc claim with *implemented?*,
*tested where?*, *deviating how?* — lives in [checklist/](checklist/).**
The headline semantics, because they are *not* C:

* **Everything is 64-bit**: "all values are extended to 64-bit when
  accessed; intermediate calculations are done with 64-bit values."
  Sub-64 variables truncate in memory and widen on access; `U64`
  operands select unsigned divide/mod/shift/compare; the assignment
  *expression* yields the **untruncated** RHS (`j1=i1=0x12345678` →
  `i1==0x5678`, `j1==0x12345678`).
* **TempleOS precedence**: `` ` ``,`>>`,`<<` | `*`,`/`,`%` | `&` | `^`
  | `|` | `+`,`-` | comparisons | `==`,`!=` | `&&` | `^^` | `||` |
  assignments — so `2+1&3 == 3` and `1<<4/2 == 8`. `` ` `` is power;
  there is no `?:`.
* **Chained comparisons**: `5<i<j+1<20`, single evaluation, short
  circuit.
* **Pointers are I64s**: `+ - += -= ++ --` move by **bytes**; indexing
  `p[i]` scales by element size; cast to step (`b=arr(U8 *); b++;`).
* **Shift counts follow x86**: masked to 6 bits, `1<<64 == 1`.
* **Scoping**: locals are function-scope `NoDups` — no C block
  shadowing (`I64 x; { I64 x; }` is an error). Globals, functions,
  classes and `#define`s are `DupsAllowed`: a dup **overshadows** (so a
  file can be re-`#include`d). Class members are `NoDupsButPad`.
  `extern` doubles as the forward reference (mutual recursion).
* **Classes are packed**: members sit back-to-back with *no* C ABI
  padding, `sizeof` is the exact byte sum; TempleOS aligns by hand with
  `pad`/`reserved` members.
* **No `main()`**: top-level statements run at startup, in order
  (`__HC_startup`; AOT wraps it in a `main`).

Deliberate deviations live in one place —
**[DEVIATIONS.md](DEVIATIONS.md)**: *documented limitations* (TempleOS
machinery a hosted compiler cannot have) and *doc-silent judgment
calls* (decisions where the doc is silent, pinned by tests but not
HolyC guarantees).

## The stdlib

Minimal but faithful to TempleOS names and behaviors — `Print` with the
TempleOS fmt codes, `PutChars`, `GetStr`/`GetI64`/`YorN`, `MAlloc`/
`Free` (NULL ok!)/`MSize`, `Str*`, the math library, `Seed`/`Rand*`,
`Bt`/`Bts`/`Btr`/`Btc`/`BCnt`, `tS`/`Now`/`Sleep`, `FileRead`/
`FileWrite`, `throw`/`PutExcept`/`Fs`, and `StreamPrint` for `#exe {}`.
Runtime symbols live in `hcrt` under `HC_*` names; the prelude binds
them to their HolyC names with `_extern`, exactly how TempleOS binds C
to asm. **The full reference — every signature, default arg, fmt code
and deviation — is [lib/README.md](lib/README.md).**

## Testing

`tests/run_tests.sh` (also `ninja -C build check`) runs:

* **golden tests** in `tests/cases/` — each `.HC` runs under **JIT and
  AOT, each at both -O0 and -O2** (four runs), and every run must
  byte-match the same `.out` (per-mode `.jit.out`/`.aot.out` for
  `#ifjit`/`#ifaot`); stdin fixtures via `.in`. Optimization must never
  change observable behavior, and the suite enforces it;
* **error tests** in `tests/errors/` — must fail with the message named
  in their `//ERR:` header;
* **edge cases** in `tests/edge/` — common *and* uncommon patterns,
  including deliberately-broken code. A file with a `//ERR:` header is an
  error test (must be rejected cleanly with that diagnostic, never crash);
  any other file is a golden test run under **both JIT and AOT**. ctest
  auto-classifies each file;
* **anti-C tests** in `tests/anti_c/` — same auto-classification as
  `tests/edge/`; see below;
* **front-end goldens** in `tests/frontend/` — `--dump-tokens` /
  `--dump-ast` output.

The cases include the doc's examples verbatim (widening, `NullCase`,
`SubSwitch`, `SubIntAccess`, hello-world variants) plus systematic
coverage of precedence, chaining, defaults/skipped args, varargs,
classes, exceptions, sub-int access, pointers, the preprocessor, `#exe`,
strings/memory/math, time/files, and I/O — every function the prelude
exposes is exercised by at least one golden (which, since goldens run
under JIT and AOT, also proves each runtime symbol is wired into both
the JIT symbol map and `libhcrt.a`).

The **edge** category stress-tests corners: integer overflow/wraparound
and 64-bit widening boundaries, x86 shift-count masking (`1<<64==1`),
precedence/unary chains, scoping (NoDups), deep & mutual recursion,
pointer aliasing / linked lists, switch extremes (ranges, sub_switch,
null cases, fallthrough), string escapes / embedded nulls / 8-char
consts / format edges, deep inheritance / union punning / nested member
chains, goto spaghetti / do-while-once, defaults / skipped / empty
varargs / fn-ptr arrays, numeric boundaries — plus 12 broken programs
that must each be rejected with a clear diagnostic and no crash. Each
edge test's asserted behavior is checked against the HolyC docs (not C):
genuinely-unspecified corners (e.g. evaluation order) are labeled
hcc-defined rather than presented as HolyC guarantees.

The **anti_c** category exists because the most dangerous bugs in a
HolyC compiler are C habits: each test guards a place where C semantics
would be *wrong* — packed class layout with hand-`pad`ding (no C ABI
padding, ever), duplicate class definitions overshadowing (no
one-definition rule), `%` on F64 (C forbids it), default args evaluated
at the call site (C has none), and `extern` forward references (not C
prototypes). When an audit finds a projected C-ism, the fix lands
together with a new `anti_c` test.

## Contributing

See [DEVELOPMENT.md](DEVELOPMENT.md) for the development philosophy
(spec fidelity rules, JIT/AOT parity, stdlib policy, checklists).
