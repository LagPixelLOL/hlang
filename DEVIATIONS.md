# Deviations from TempleOS HolyC

The two normative lists of everything `hcc` deliberately does
differently from TempleOS. Anything not listed here is a bug (see
DEVELOPMENT.md: a silent divergence is a bug; a documented judgment
call is engineering). The per-claim mapping of these deviations onto
the HolyC docs lives in [checklist/](checklist/).

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

## Doc-silent judgment calls (hcc-defined)

Where the archived doc is silent, hcc picks the interpretation most
consistent with the 64-bit doctrine, pins it with tests, and lists it
here (per DEVELOPMENT.md these are *not* HolyC guarantees):

* `do {} while ();` is implemented (the doc never mentions it).
* Brace initializer lists for arrays/classes, at global and local scope;
  partial lists zero-fill the rest. Multi-dimensional arrays, and
  `sizeof(expr)` on arbitrary expressions.
* `%` accepts F64 operands (one numeric tower; lowers to fmod).
* Default args are compiled into each **call site** and re-evaluated
  per call — genuine TempleOS behavior (`U0 GrLine(CDC *dc=gr_dc,...)`
  defaults to the *live* global), just undocumented.
* F64→I64 conversion truncates toward zero, following the doc's
  "TempleOS follows normal C float<-->int conversion". (Real TempleOS
  is often reported to round-to-nearest via CVTSD2SI; the doc's wording
  wins here. `ToI64()` matches.)
* Numeric-literal **extensions** (accepted by hcc, not HolyC): `0b`
  binary literals, `_` digit separators.
* `Print` superset: `%u`, lowercase `%x`, C-escape superset
  (`\a \b \f \v`); unknown fmt codes pass through literally.
* Argument/operand evaluation order is left-to-right (deterministic and
  identical under JIT/AOT) but is **not** asserted as a HolyC guarantee
  — the docs never promise one.
