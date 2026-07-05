# benchmarks/

Paired, algorithm-identical compute kernels — `NAME.HC` compiled by
`hcc`, `NAME.c` compiled by the system `cc` — timed against each other.

```sh
ninja -C build bench                    # or:
HCC=build/hcc benchmarks/run_benchmarks.sh [name-filter]
REPS=5 benchmarks/run_benchmarks.sh     # more repetitions (default 3)
```

**Not part of the test suite.** `ninja check`/`tests/run_tests.sh`
never touch this directory, and benchmarks never gate a commit. And
unlike the test suite (which ctest runs multithreaded), everything here
runs **strictly sequentially** — one compile, one timed process at a
time — because parallelism poisons benchmark numbers.

## Methodology

* Both sides compile at **-O3** (`hcc -O3` AOT vs `cc -O3`).
* **Correctness gate before timing**: each kernel prints a
  deterministic checksum, and the HolyC and C binaries must produce
  byte-identical output or the run fails — timing garbage is worthless.
  (Checksums are integers, and the F64 kernels are built from exact
  IEEE operations, so bit-identical results are a hard requirement, not
  a hope.)
* **Best-of-N wall time** (default N=3) per column:
  * `hcc-jit` — `hcc -O3 file.HC`, the TempleOS-style edit-run cycle;
    this column *includes compile time* by nature.
  * `hcc-aot` — the pre-built `hcc -O3 -o` native binary.
  * `cc` — the pre-built C binary.
  * `aot/cc` — the ratio that answers "how far from C is hcc's codegen?"

## Why the jit column is so much bigger than aot

The jit number carries a fixed latency the kernel never sees, measured
(on the reference box) as roughly:

| cost | ~ms | what it is |
|---|---|---|
| process startup | 90 | **static initializers in the statically-linked LLVM** — an empty `main()` linking the same components pays the same; the dynamic loader itself is ~3 ms. Not removable without rebuilding LLVM |
| front end | 25 | lex+parse+codegen of the prelude + program |
| `-O3` IR pipeline | 75 | `buildPerModuleDefaultPipeline(O3)` over the module (AOT pays this too, but off the clock) |
| ORC materialization | 60 | instruction selection/relocation of every function at lookup time |

So `hcc file.HC` answers "how fast is the edit-run cycle?" (~200-350 ms
to compile *and* run a kernel at -O3 — TempleOS-grade interactivity),
while `hcc-aot` vs `cc` is the honest codegen-quality comparison.
Compiler-side codegen quality work should watch the `aot/cc` column;
edit-cycle work should watch `jit` minus the kernel's own runtime.

## Kernels

| kernel | pattern |
|---|---|
| `fib` | naive recursion — pure call/return overhead (`Fib(34)`) |
| `sieve` | branchy byte-array marking, 50 passes over 200k |
| `matmul` | naive 256×256 F64 matrix multiply (exact dyadic inputs) |
| `hash` | FNV-1a over 1 MiB × 100 — byte loads + wrapping 64-bit multiply |
| `sort` | quicksort of 2M I64s from a deterministic xorshift64 stream |
| `mandel` | Mandelbrot escape-iteration sum — data-dependent F64 loop |

## Fairness rules (for adding kernels)

1. Same algorithm, same data, same operation order in both files — the
   C file is a *mirror*, not an optimized rival. Idiomatic divergence
   (SIMD intrinsics, `qsort(3)`, manual unrolling) belongs in neither.
2. Deterministic integer checksum printed once at the end (`"%d\n"` ↔
   `printf("%lld\n", (long long)...)`), no other output.
3. Seed any randomness with a fixed in-file constant (xorshift64 above),
   never the stdlib RNGs — the two languages must compute the same
   stream.
4. HolyC-side reminders: locals are `NoDups` (declare cursors once),
   there is no `continue`, pointer `+`/`-` is unscaled (use indexing,
   which scales in both languages).
5. Target roughly 0.05–0.5 s per run at -O3 — long enough to time,
   short enough that best-of-3 stays pleasant.

C files follow the repo `.clang-format`; HolyC files follow TempleOS
style (see DEVELOPMENT.md).
