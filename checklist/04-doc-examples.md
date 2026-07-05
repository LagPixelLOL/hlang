# Checklist: the doc's example programs (`holyc-docs/HC/*.HC`, `helloworld.HC`)

DEVELOPMENT.md rule #1: doc examples with expected output are contract
tests. This file records, per shipped example, whether it runs
**verbatim** under hcc, where that is pinned, and why not where not.

| # | example | runs verbatim | tested | deviations |
|---|---|---|---|---|
| 1 | `helloworld.HC` — 5 pure-HolyC variants (bare string, `Main`, `MyPrint "%s"`, `MyPrint2`, `"" st`) | yes | `cases/hello` (all five, plus the char-const/`PutChars` material) | none |
| 2 | `helloworld.HC` — 4 asm variants (`DU8`, `PUT_STR`, `RET1`, `_extern _MY_PRINT` over asm) | no (documented) | `errors/asm` (clear rejection) | inline `asm {}` is out of scope; LLVM owns codegen (README limitation). The `_extern`-binds-a-symbol *mechanism* itself is implemented and is how the whole stdlib is bound |
| 3 | `NullCase.HC` — null cases count up from zero | yes | `cases/switch` (verbatim, output for i=0..19 golden-matched) | none |
| 4 | `SubSwitch.HC` — `start:`/`end:` porches, `>Zero [One] Two [Three] Four [Five]` | yes | `cases/subswitch` (verbatim; documented output byte-matched) | none |
| 5 | `SubIntAccess.HC` — `q.i16[2]`, `q.u8[5]`, `q.i32[1].u8[2]`, `q.i32[0].i8[1]` | yes | `cases/subint` (verbatim, all four documented `%016X` results) | none |
| 6 | `Exceptions.HC` — `Prompt`/`YorN`, `throw('Point1')`, `Fs->except_ch`, `Fs->except_callers[0]`, `catch_except` gating | yes | *probe* (runs verbatim, driven by piped `y`/`n`; handler routing matches the doc) — semantics pinned instead by `cases/exceptions` + `examples/Oracle.HC` | its `%P,Fs->except_callers[0]` prints live return addresses → nondeterministic, cannot be a byte-exact golden |
| 7 | `LastClass.HC` — `StructName(...,lastclass)`, then `ClassRepD(Gs)`/`ClassRep(Fs)` | partial | the `lastclass` half is `cases/classes` (Student/School/I64/F64/`Fs` names verbatim); `PressAKey` works (`cases/input`) | `ClassRep`/`ClassRepD` (and `Gs`) are compiler reflection — not exposed, so the file as a whole is rejected (README limitation) |
| 8 | `ClassMeta.HC` — `HashFind(...,Fs->hash_table,HTT_CLASS)`, `MemberMetaData`, `MemberMetaFind`, meta-driven `DumpStruct` | no (documented) | meta-data *parsing* is pinned by `cases/classes` (`print_str`/`dft_val`/`percentile` accepted) | reflection API (`CHashClass`, `CMemberLst`, `Fs->hash_table`) not exposed; meta data is parsed and discarded (README limitation) |
| 9 | `Directives.HC` — `__DATE__`/`__TIME__`/`__FILE__`/`__DIR__`/`__LINE__`/`__CMD_LINE__` + `"%P:%08X",$,$` | partial | all six directives: `cases/preproc`, `cases/mode`, `examples/CompileTime.HC`; the `$` lines: `errors/dollar` | everything except `$`-as-instruction-address works; `$` in expressions is rejected with a clear error (README limitation) |
| 10 | `Lock.HC` — `lock`/`lock{}` on `glbl++`, `JobQue`/`JobResGet` two-core race demo | partial | `lock {}` and brace-less `lock`: `cases/misc` | body compiles without LOCK prefixes (single-threaded runtime); `JobQue`/`JobResGet` don't exist, so the file as a whole doesn't run (README limitation) |
| 11 | `StkGrow.HC` — `CallStkGrow(0x800,...,&Recurse,...)`, 2M-deep recursion | no (documented) | deep recursion itself: `edge/edge_recursion` (50k frames, no ceremony) | host stacks grow, so `CallStkGrow` is unnecessary and not provided (README limitation); the doc's motivating problem does not exist here |

## Contract-test status

The four language examples the top-level README promises "run
unmodified" — `NullCase.HC`, `SubSwitch.HC`, `SubIntAccess.HC`, the
hello-world HolyC variants — are all golden tests and never change
(rows 1, 3, 4, 5). `Exceptions.HC` also runs unmodified but is
input-driven and address-printing, so its guarantee is manual (row 6).
The remaining examples exercise TempleOS machinery (assembler,
reflection, multicore, task stacks) that is deliberately out of scope;
each is rejected cleanly and its *language-level* content is covered by
other tests, as itemized above.
