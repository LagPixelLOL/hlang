# Source-of-truth conformance checklist

Item-by-item audit of `hcc` against the HolyC source-of-truth documents
in [holyc-docs](https://github.com/SpaciousCoder78/holyc-docs) — the archive
of `templeos.holyc.xyz/Wb/Doc/HolyC.html` plus the CompilerOverview
pages. Every claim the docs make is listed with three answers:

* **Implemented** — does hcc do it? (`yes` / `partial` / `no (documented)`)
* **Tested** — which test pins it. *probe* means it was verified by
  running hcc during the audit but is not (or cannot be) a golden test;
  the entry says why.
* **Deviations** — any difference from the doc, and where it is
  documented (all deliberate deviations must appear in the top-level
  `DEVIATIONS.md` per DEVELOPMENT.md rule #1).

| file | source document |
|---|---|
| [01-holyc-language.md](01-holyc-language.md) | [holyc-docs/README.md](https://github.com/SpaciousCoder78/holyc-docs/blob/main/README.md) (the main HolyC doc) |
| [02-scoping-linkage.md](02-scoping-linkage.md) | [CompilerOverview/scopinglinkage.md](https://github.com/SpaciousCoder78/holyc-docs/blob/main/CompilerOverview/scopinglinkage.md) |
| [03-preprocessor-directives-options.md](03-preprocessor-directives-options.md) | [CompilerOverview](https://github.com/SpaciousCoder78/holyc-docs/tree/main/CompilerOverview)`/{preprocessor,directives,options}.md` |
| [04-doc-examples.md](04-doc-examples.md) | [HC/](https://github.com/SpaciousCoder78/holyc-docs/tree/main/HC)`*.HC` + [helloworld.HC](https://github.com/SpaciousCoder78/holyc-docs/blob/main/helloworld.HC) example programs |

## Summary (as of this audit)

* **Language**: everything the main doc specifies is implemented and
  golden-tested, with one behavior class deliberately divergent
  (`switch []` still bounds-checks) and one behavior doc-ambiguous
  (F64→I64 conversion truncates per the doc's own "normal C conversion"
  wording; real TempleOS is reported to round). Diagnostics quote the
  doc for every "there is no X" clause (`continue`, `typedef`, `?:`,
  `#define` functions, `#include <>`, `asm`, `$`).
* **Scoping table**: every dup-rule row is now enforced and tested —
  local `NoDups`, global/function/class/`#define` `DupsAllowed`
  (overshadow), class member `NoDupsButPad`, goto labels `NoDups`.
  The task/hash-table scope model itself is hosted-out-of-scope.
* **Preprocessor/directives/options**: fully implemented except the
  TempleOS-task-dependent parts (`Cd()`/`StreamDir` inside `#exe`);
  `Option()`/`OPTf_*` are accepted no-ops.
* **Doc examples**: the four language examples + five HolyC hello-world
  variants run verbatim; the asm/reflection/multicore examples are
  rejected with clear diagnostics, as documented.

Fixes that fell out of this audit: dup class members were silently
accepted (now `NoDupsButPad` per the table), duplicate goto labels
crashed codegen with invalid IR (now a clean error), `typedef` and `?:`
got doc-quoting diagnostics, `__CMD_LINE__` is pinned per mode, and
`no_warn`/`reg R15`/function flags/`lock` (brace-less too)/`Option()`
gained golden coverage (`tests/cases/misc.HC`).
