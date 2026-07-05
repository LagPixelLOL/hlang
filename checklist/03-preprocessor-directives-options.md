# Checklist: preprocessor, directives, options ([CompilerOverview](https://github.com/SpaciousCoder78/holyc-docs/tree/main/CompilerOverview)`/{preprocessor,directives,options}.md`)

## preprocessor.md

| # | doc claim | implemented | tested | deviations |
|---|---|---|---|---|
| 1 | No separate preprocessor pass — `Lex()` has it built in | yes (same architecture) | `frontend/tokens` (macros expand inside the token dump) | none |
| 2 | Compiler looks ahead a token; "throw an extra semicolon after a directive" if it lags | yes | `cases/exe` (`#exe` splices mid-stream) | hcc's lexer applies directives immediately; the extra-`;` workaround is never needed but harmless |
| 3 | `#include ""` — **no `<>` form** | yes | `cases/preproc` (+ `preproc_inc.HH`), `errors/include_angle` | none |
| 4 | `#exe {}` executes at compile time, `StreamPrint()` inserts source; `#exe {Cd("DirName");;}` idiom | partial | `cases/exe` (consts, statements, `#define` injection), `examples/CompileTime.HC` (tables, functions, strings) | `StreamPrint` fully supported; `Cd()`/`StreamDir` are TempleOS-task facilities and are not provided (DEVIATIONS.md) |
| 5 | `#define` — define string const (object-like) | yes | `cases/preproc` (incl. recursive expansion, redefinition) | none |
| 6 | `#assert` — warn during compilation if expression untrue | yes | `cases/preproc` (true case is silent); false case *probe* — prints `warning: #assert failed`, compilation continues | warning text contains the source path → the false case cannot be a byte-exact golden |
| 7 | `#if` / `#else` / `#endif` | yes | `cases/preproc` (nesting, garbage tokens in dead branches) | none |
| 8 | `#ifdef` / `#ifndef` | yes | `cases/preproc` | none |
| 9 | `#ifaot` / `#ifjit` — select on compiler mode | yes | `cases/mode` (per-mode goldens `.jit.out`/`.aot.out`) | none — this is the one *sanctioned* JIT/AOT divergence |
| 10 | `defined()` usable in expressions | yes | `cases/preproc` (`#if CNT>10 && defined(NAME) \|\| 0`) | none |
| 11 | `#help_index`, `#help_file` (Help System) | accepted, ignored | `cases/preproc` | there is no Help System to feed (DEVIATIONS.md) |

## directives.md (the `__X__` builtins)

The doc shows these as `#define`s expanding to `#exe{...}` over
TempleOS internals (`Fs->last_cc->...`). hcc implements them as lexer
builtins with the same observable results — necessarily, since
`Fs->last_cc` does not exist (DEVIATIONS.md).

| # | directive | implemented | tested | deviations |
|---|---|---|---|---|
| 12 | `__DATE__` (`"%D",Now`) | yes | `examples/CompileTime.HC`; *probe* | builtin, not an `#exe` macro; value is compile-time wall clock → not goldenable |
| 13 | `__TIME__` (`"%T",Now`) | yes | same | same |
| 14 | `__LINE__` | yes | `cases/preproc` (`line=%d` golden) | none |
| 15 | `__CMD_LINE__` (compiled from cmd line, depth<1) | yes | `cases/mode` (`1` under JIT, `0` in an AOT module — per-mode goldens) | faithful to the `CCF_CMD_LINE` spirit in a hosted world |
| 16 | `__FILE__` | yes | `cases/preproc` (asserts the path contains `.HC`) | none |
| 17 | `__DIR__` (`StreamDir`) | yes | `cases/preproc` (asserts non-empty — value is invocation-path dependent) | builtin; standalone `StreamDir` is not provided (DEVIATIONS.md) |

## options.md (`Option()` / `OPTf_*`)

`Option()` is accepted with any `OPTf_*` flag and is a no-op: the flags
either configure TempleOS machinery hcc doesn't have, or request
warnings hcc doesn't emit. All constants exist in the prelude so doc
code parses. (DEVIATIONS.md.)

| # | option | implemented | tested | deviations |
|---|---|---|---|---|
| 18 | `Option()` itself; "you might need `#exe {Option();}`" | accepted no-op | `cases/misc`, `cases/arrays`; `#exe{}` can call it (`cases/exe` machinery) | no compiler state to flip |
| 19 | `OPTf_GLBLS_ON_DATA_HEAP` (2-Gig code heap, .BIN size) | N/A | — | no code/data heap split, no `.BIN` format; globals are ordinary native globals |
| 20 | `OPTf_EXTERNS_TO_IMPORTS`, `OPTf_KEEP_PRIVATE` (dual-use kernel headers) | N/A | — | `import` ≡ `extern` in hcc already (see scoping checklist #5) |
| 21 | `OPTf_WARN_UNUSED_VAR` | no-op | `cases/misc` (via `Option()` acceptance) | unused-var warnings not implemented (`no_warn` is likewise a no-op) |
| 22 | `OPTf_WARN_PAREN` | no-op | `cases/arrays` | paren lint not implemented |
| 23 | `OPTf_WARN_DUP_TYPES` | no-op | `cases/misc` | dup-type lint not implemented |
| 24 | `OPTf_WARN_HEADER_MISMATCH` | no-op | — (constant exists) | header-mismatch check not implemented |
| 25 | `OPTf_NO_REG_VAR` (force locals to stack) | no-op | `cases/misc` | register allocation is LLVM's (same reason `reg`/`noreg` are ignored) |
| 26 | `OPTf_NO_BUILTIN_CONST` (disable 10-byte float consts for `pi`, `log2_10`, `log10_2`, `loge_2`) | N/A | `cases/floats` (`pi` value) | the four consts exist as F64 prelude `#define`s; there are no 80-bit floats to disable (DEVIATIONS.md) |
