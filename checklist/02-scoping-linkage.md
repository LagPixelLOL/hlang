# Checklist: scoping & linkage ([CompilerOverview/scopinglinkage.md](https://github.com/SpaciousCoder78/holyc-docs/blob/main/CompilerOverview/scopinglinkage.md))

The scoping doc has two parts: prose about the JIT symbol model and
`extern`/`import`/`_extern`/`_import`, then the AOT/JIT scoping tables.
The dup-rules column of those tables is fully enforced; the TempleOS
task/hash-table machinery itself is a documented hosted divergence.

## Symbol model & linkage keywords

| # | doc claim | implemented | tested | deviations |
|---|---|---|---|---|
| 1 | JIT uses the task's hash sym table + parent tasks' tables; syms scope like environment vars | no (documented) | — | hosted hcc compiles one program per invocation; there are no tasks. `extern`/`import` resolve against the process/link instead (DEVIATIONS.md) |
| 2 | A newly placed sym **overshadows** older syms of the same name | yes | `anti_c/anti_c_class_redef` (function, global, class) | none |
| 3 | Dups allowed *by design* so a file can be re-`#include`d / `DrawIt()` redefined | yes | `anti_c/anti_c_class_redef`, `cases/preproc` (`#define` redefined without `#undef`) | none |
| 4 | `extern` binds to an existing same-name sym; "also can be used to generate a fwd reference" | yes | `anti_c/anti_c_fwd_extern` (mutual recursion + fwd-referenced global defined later) | bare C-style prototypes are tolerated as the same thing (DEVIATIONS.md) |
| 5 | `import` binds at `Load()` time; incomplete until defined | partial | accepted (prelude parse path) | there is no `Load()` in a hosted world: `import` behaves like `extern` (DEVIATIONS.md) |
| 6 | `_extern` binds a sym of a **different name** ("binds C to asm") | yes | `lib/HolyC.HH` binds the entire stdlib via `_extern HC_*`; every stdlib golden exercises it | none — this is exactly how hcc binds HolyC names to hcrt |
| 7 | `_import` — different-name bind at `Load()` time | partial | accepted | ≡ `_extern` in the hosted world (DEVIATIONS.md) |

## Scoping-table rows (the dup-rules column)

Legend from the doc: `D` = DupsAllowed (dup overshadows), `N` = NoDups,
`P` = NoDupsButPad, `W` = WarningUnlessClosedOut.

| # | table row | rule | implemented | tested | deviations |
|---|---|---|---|---|---|
| 8 | `#define x` | D | yes | `cases/preproc` (redefine without `#undef` → overshadows) | none |
| 9 | `function` | D | yes | `anti_c/anti_c_class_redef` (`F()` redefined → later wins) | none |
| 10 | local `var` | **N** | yes | `edge/edge_dup_local` (block shadowing rejected!), `edge/edge_scoping` | none — this is the flagship not-C rule |
| 11 | `static var` | StaticVarInit only, N | yes | `edge/edge_scoping` (const init, persistence), `errors/static_dyn_init` (dynamic init rejected with a clear message) | none |
| 12 | global `var` | dynamic init allowed, **D** | yes | `cases/globals` (dyn init in program order), `anti_c/anti_c_class_redef` (dup global re-inits at its position) | none |
| 13 | `class` | **D** | yes | `anti_c/anti_c_class_redef` (overshadow; earlier instances keep the old layout) | *fixed during this audit* — hcc previously projected C's one-definition rule and errored |
| 14 | `class member` | **P** (NoDupsButPad) | yes | `errors/dup_member` (rejected), `anti_c/anti_c_class_pack` (multiple `pad` allowed) | *fixed during this audit* — dups were silently accepted before |
| 15 | C `goto label:` | function scope, N | yes | `cases/control`, `edge/edge_control` (spaghetti), `edge/br_dup_label` (dup rejected) | *fixed during this audit* — a duplicate label previously crashed codegen with invalid IR |
| 16 | `asm` label rows (export/local/IMPORT) | — | no (documented) | `errors/asm` | inline asm is out of scope entirely (DEVIATIONS.md) |
| 17 | `extern function` | W (WarningUnlessClosedOut) | partial | `errors/undefined_fun` (call of a never-defined name is a hard error) | in hosted JIT an unresolved sym is an error at materialization, not a warning; AOT defers to the linker |

## Prose notes at the bottom of the doc

| # | doc claim | implemented | tested | deviations |
|---|---|---|---|---|
| 18 | Goto labels must not collide with global names ("baffling" errors) | N/A — better | `edge/edge_control` | hcc keeps labels in a function-local namespace; collisions are impossible, so the baffling TempleOS failure mode does not exist |
| 19 | `pad`/`reserved` members special-cased | yes | `anti_c/anti_c_class_pack` | none |
| 20 | `reg`/`noreg` override reg-var allocation; optional named reg | parsed | `cases/misc` | LLVM owns register allocation; annotations are accepted and ignored (DEVIATIONS.md) |
| 21 | Local vars accessible in asm via `&i[RBP]`; globals via `[&glbl_var]` | no | — | asm out of scope (DEVIATIONS.md) |
| 22 | Offspring tasks inherit syms | no | — | no tasks (DEVIATIONS.md) |
| 23 | `sizeof()`/structure members usable in asm blks | no | — | asm out of scope |
| 24 | `&i` or `i.u8[2]` forces a local to `noreg`; try/catch forces all locals `noreg` | N/A | `cases/subint` (locals), `cases/exceptions` | with LLVM regalloc the constraint is meaningless; semantics (address-taken locals, live-across-setjmp values) are correct by construction |
| 25 | An unused stack gap is left for reg vars | N/A | — | TempleOS stack-frame detail |
| 26 | Static function vars don't go on the data heap | N/A | — | hosted memory model has no code/data heap split |
| 27 | `OPTf_EXTERNS_TO_IMPORTS` treats externs as imports (JIT/AOT dual headers) | accepted (no-op) | `cases/misc` (`Option()` path) | unnecessary in hcc: `import` ≡ `extern` already (DEVIATIONS.md) |
