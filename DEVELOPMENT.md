# hlang development guide

How this codebase is meant to be grown. Read this before adding features
or "fixing" behavior that looks wrong.

## Philosophy

### 1. The spec is the docs, and the docs are Terry's

The language definition lives in `../holyc-docs` (an archive of the
TempleOS HolyC documentation). It is the **single source of truth** for
language behavior:

* If the doc gives an example with expected output (the 64-bit widening
  block, `NullCase.HC`, `SubSwitch.HC`, `SubIntAccess.HC`, the
  hello-world variants), that exact program with that exact output must
  live in `tests/cases/`. These are contract tests — they never change.
* If the doc says something odd — `&` binding tighter than `+`, no
  `continue`, assignment yielding the *untruncated* RHS, null cases
  counting up from zero — the odd thing is the feature. Do not
  "normalize" HolyC toward C. HolyC is not C.
* If the doc is silent (e.g. pointer arithmetic scaling), pick the
  interpretation most consistent with the doctrine that *"all values
  are extended to 64-bit; intermediate calculations are done with
  64-bit values"* (pointers are just I64s → unscaled `+`/`-`, scaled
  indexing), implement it once, test it, and **record the decision in
  the matching `checklist/` row** (plus DEVIATIONS.md's judgment-calls
  list when it is user-visible). A silent divergence is a bug; a
  documented judgment call is engineering.
* The per-claim audit trail lives in `checklist/` — one row per doc
  claim with *implemented?* / *tested where?* / *deviations*. When you
  add or change doc-visible behavior, update the matching checklist row
  in the same commit. The two normative deviation lists (limitations,
  judgment calls) live in `DEVIATIONS.md`; the README keeps only the
  headline semantics.

### 2. Two backends, one behavior

Every observable behavior must be identical under JIT and AOT — and at
every optimization level. That is why the test runner executes every
golden test in **both** modes, at **-O0 and -O2**, and diffs all four
runs against the same file. The only permitted divergence is
`#ifjit`/`#ifaot` (which exists precisely to express that divergence in
user code) — tested with per-mode goldens (`.jit.out`/`.aot.out`).

Never special-case "works in JIT, skip in AOT" or vice versa. If a
feature can only work in one mode, it must be a *documented limitation*
and produce a clear diagnostic in the other mode.

### 3. TempleOS spirit for the stdlib, exact spec for the language

These are different standards, on purpose:

* **Language** (lexer/parser/codegen): exactness. Match documented
  behavior bit-for-bit. Any deviation is either a bug or a documented
  limitation with a clean error message — recorded in the matching
  `checklist/` row, and in DEVIATIONS.md's limitations list when
  user-facing.
* **Stdlib** (`runtime/hcrt.c` + `lib/HolyC.HH`, reference in
  `lib/README.md`): fidelity of *names and feel*, hosted reality of
  *implementation*. `Free(NULL)` is fine
  because Terry said so. `MSize` exists. `GetStr` returns a MAlloc'ed
  line. `Print` speaks `%z` and comma-flags. But underneath it's libc,
  and that's fine — TempleOS's VGA, tasks, and heaps are not being
  emulated. Keep it minimal: add a function when a real program or test
  needs it, never speculatively, and always with the TempleOS name and
  signature (check the TempleOS sources/docs before inventing one).
* The prelude binds runtime symbols with `_extern HC_Name ...` — the
  same mechanism TempleOS uses to bind C to asm. Symbols in C are
  `HC_*`; names in HolyC are Terry's. Where a routine is expressible in
  plain HolyC (`ToUpper`, `StrOcc`), write it in HolyC in the prelude —
  the stdlib should eat its own dog food.

### 4. Self-containedness is a feature

`hcc` ships as: one binary (LLVM/zstd/zlib/libstdc++ linked statically)
+ `lib/` prelude + `libhcrt.a`, all found relative to
`/proc/self/exe`. AOT output depends on libc/libm only.

* Never add a runtime dependency on LLVM shared libs, Python, or
  anything not present on a bare Linux box. The one tolerated external
  is the system `cc` for final linking (overridable via `HCC_CC`).
* JIT resolves runtime symbols via an **explicit absolute-symbol map**
  (`defineRuntimeSymbols`), not `-rdynamic`, so static linking keeps
  working. When you add a runtime function, add it to that map, or JIT
  breaks while AOT works — the test suite catches this because it runs
  both.

### 5. Simple internal models beat clever ones

Deliberate simplifications that keep the compiler small and correct:

* **Value model**: every scalar rvalue is `i64` or `double`. Pointers
  are `i64` until a load/store/GEP needs them. Widths exist only in
  memory. This *is* the HolyC semantic model, so the code and the spec
  agree by construction.
* **Aggregates are `[N x i8]`** with byte GEPs and manually computed
  offsets. No parallel LLVM struct types to keep in sync; `offset()`
  and sub-int access fall out for free.
* **No sema pass.** Parsing needs type *names* only (to disambiguate
  declarations and postfix casts); everything else resolves in codegen.
  HolyC has "no type-checking" — a permissive single pass matches the
  language's nature. Resist adding a type checker; add targeted
  diagnostics instead.
* Prefer a boring lowering (setjmp frames for try/catch, a select-var
  dispatch for sub_switch porches) over a clever one. LLVM's optimizer
  exists; correctness at `-O0` comes first — and `-O2` must not change
  observable behavior: the test suite runs every golden under JIT and
  AOT at both `-O0` and `-O2` against the same expected bytes. Never
  lie to LLVM (alignment, aliasing) "because it works at -O0" — the
  `-O2` runs exist to catch exactly that.

### 6. Errors speak HolyC

Diagnostics should sound like the docs where possible: `continue` says
*"there is no 'continue' stmt in HolyC -- use goto"*; `#include <...>`
says there is no `<>` form; `#define F(x)` quotes "no #define functions
exist". A user coming from the docs should recognize the compiler as an
implementation of *that* document. Every diagnostic worth having is
worth an `//ERR:` test in `tests/errors/`.

## Practical rules

* **Commit in reviewable increments** — one subsystem or behavior per
  commit, tests green at every commit (`ninja -C build check`). The
  history of this repo (skeleton → lexer → parser → runtime → codegen →
  tests → hardening) is the intended template.
* **Test-first for behavior questions.** When unsure what some HolyC
  construct does, write the golden test from the doc first, then make
  it pass. If the doc doesn't answer: decide, record the call in
  `checklist/` (and DEVIATIONS.md's judgment-calls list), test.
* **Don't project C onto HolyC.** For every test you write, ask: is this
  behavior actually HolyC (doc / x86 / the recursive-descent structure),
  or am I assuming C? Corners that C leaves unspecified/UB (argument and
  operand evaluation order, `i++ + ++i`, unary-vs-backtick precedence)
  are *not* HolyC guarantees — either avoid them, or label the golden as
  hcc-defined. Places where HolyC genuinely differs from C must win:
  local vars are `NoDups`/function-scope (no block shadowing), globals
  overshadow, shifts mask the count to 6 bits (x86), assignment yields
  the *untruncated* RHS, pointers are unscaled I64s, classes are
  **packed** (no C ABI padding — TempleOS pads by hand with `pad`
  members), duplicate class definitions **overshadow** (no ODR). The
  `tests/edge/` suite pins the corners; `tests/anti_c/` exists purely to
  assert HolyC-vs-C divergences. When an audit finds a projected C-ism,
  fix it and land a new `anti_c` test in the same commit; extend both
  suites, don't dilute them.
* **Every bug fix gets a test** in the same commit. No exceptions —
  regressions in a compiler are silent and brutal.
* **Formatting**: `clang-format -i src/*.cpp src/*.hpp` (4-space indent,
  left-aligned pointers, config in `.clang-format`). C runtime follows
  the same style; HolyC code in `lib/`/`tests/` follows TempleOS style
  (2-space, `U0 Fun()` braces on their own line, terse).
* **New source files** are picked up by the CMake glob; keep compiler
  code exception-free (`-fno-exceptions -fno-rtti`) and use
  `Expected`/error returns, LLVM-style.
* **Don't grow the driver surface casually.** `hcc file.HC`,
  `-o`, `-c`, `--emit-llvm`, dumps, `-I`, `-O` — that's the tool. It is
  a compiler, not a build system.

## Where things live

| concern | file |
|---|---|
| tokens, preprocessor, `#exe` capture, builtin `__DATE__`-style macros | `src/lexer.{hpp,cpp}` |
| grammar, precedence tiers, print-statement sugar, declarations | `src/parser.cpp` |
| types, class layout | `src/ast.{hpp,cpp}` |
| semantics: widening, ops, calls/defaults/varargs, switch/sub_switch, try/catch, globals | `src/codegen.cpp` |
| JIT, runtime symbol map, `#exe` snippet execution | `src/jit.cpp` |
| object emission, linking, opt pipeline | `src/aot.cpp` |
| CLI, prelude/runtime discovery | `src/driver.cpp`, `src/main.cpp` |
| runtime/stdlib C side | `runtime/hcrt.{h,c}` |
| stdlib HolyC surface + reference doc | `lib/HolyC.HH`, `lib/README.md` |
| test runner + suites | `tests/` |
| per-claim doc-conformance record (implemented/tested/deviations) | `checklist/` |
| the two normative deviation lists (limitations, judgment calls) | `DEVIATIONS.md` |
| commented example programs | `examples/` |

## Adding a stdlib function (checklist)

1. Implement `HC_Foo` in `runtime/hcrt.c`, declare in `runtime/hcrt.h`.
2. Register it in `defineRuntimeSymbols()` (`src/jit.cpp`).
3. Bind it in `lib/HolyC.HH`: `_extern HC_Foo RetType Foo(args);`
   with TempleOS-faithful name, defaults, and argument order (check the
   TempleOS sources/docs before inventing one).
4. Document it in `lib/README.md` (signature, defaults, deviations).
5. Add/extend a golden test in `tests/cases/` (it runs in both modes).
6. `ninja -C build check`, commit.

## Adding a language feature (checklist)

1. Find the doc passage; put its example in a test verbatim.
2. Lexer → parser (AST node if needed) → codegen. Keep the value model.
3. Negative tests for new diagnostics.
4. Update the matching `checklist/` row (implemented/tested/deviations);
   if partial or divergent, also DEVIATIONS.md.
5. `ninja -C build check`, commit.
