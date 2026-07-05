#!/bin/sh
# benchmarks/run_benchmarks.sh -- HolyC (hcc) vs C (system cc) on paired,
# algorithm-identical compute kernels.
#
#   HCC=build/hcc benchmarks/run_benchmarks.sh [name-filter]
#   REPS=5 ... for more repetitions (default 3, best-of)
#
# NOT part of the test suite: `ninja check`/run_tests.sh never run this.
# Everything here runs STRICTLY SEQUENTIALLY -- one compile or one timed
# process at a time -- because parallelism poisons benchmark numbers.
# Both sides compile at -O3.
#
# For each NAME.HC with a NAME.c sibling:
#   1. compile: hcc -O3 (AOT)  and  cc -O3
#   2. correctness gate: both binaries must print byte-identical output
#      (each kernel emits a deterministic checksum) -- a mismatch fails
#      the run; timing garbage is worthless
#   3. time best-of-$REPS: hcc's JIT split into LOAD (hcc --no-run:
#      startup + front end + -O3 pipeline + ORC materialization) and RUN
#      (total minus load -- the kernel's actual runtime under the JIT),
#      then the AOT binary and the C binary
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
HCC="${HCC:-$HERE/../build/hcc}"
CC="${CC:-cc}"
REPS="${REPS:-3}"
FILTER="${1:-}"

[ -x "$HCC" ] || { echo "hcc not found at $HCC (build first: ninja -C build)"; exit 1; }
command -v "$CC" >/dev/null || { echo "C compiler '$CC' not found"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

now_ns() { date +%s%N; }

# best-of-N wall time of "$@", echoed in milliseconds (x.y)
best_ms() {
    best=""
    n=0
    while [ "$n" -lt "$REPS" ]; do
        s=$(now_ns)
        "$@" > /dev/null 2>&1
        e=$(now_ns)
        d=$(( (e - s) / 100000 ))     # tenths of a millisecond
        if [ -z "$best" ] || [ "$d" -lt "$best" ]; then best=$d; fi
        n=$((n + 1))
    done
    echo "$((best / 10)).$((best % 10))"
}

fail=0
printf '%-10s %13s %12s %12s %12s %10s\n' benchmark 'jit-load(ms)' 'jit-run(ms)' 'hcc-aot(ms)' 'cc(ms)' 'aot/cc'
printf '%-10s %13s %12s %12s %12s %10s\n' ---------- ------------ ----------- ----------- ------- ------

for hc in "$HERE"/*.HC; do
    [ -e "$hc" ] || continue
    name=$(basename "$hc" .HC)
    c="$HERE/$name.c"
    [ -f "$c" ] || { echo "SKIP $name: no C mirror"; continue; }
    case "$name" in *"$FILTER"*) ;; *) continue ;; esac

    if ! "$HCC" -O3 "$hc" -o "$TMP/$name.hcc" > "$TMP/err" 2>&1; then
        echo "FAIL $name: hcc compile"; sed 's/^/    /' "$TMP/err"; fail=1; continue
    fi
    if ! "$CC" -O3 -o "$TMP/$name.cc" "$c" -lm > "$TMP/err" 2>&1; then
        echo "FAIL $name: cc compile"; sed 's/^/    /' "$TMP/err"; fail=1; continue
    fi

    # correctness gate: identical checksums or the timing means nothing
    "$TMP/$name.hcc" > "$TMP/$name.hc.out" 2>&1
    "$TMP/$name.cc"  > "$TMP/$name.c.out"  2>&1
    if ! cmp -s "$TMP/$name.hc.out" "$TMP/$name.c.out"; then
        echo "FAIL $name: HolyC and C outputs differ"
        diff "$TMP/$name.hc.out" "$TMP/$name.c.out" | sed 's/^/    /'
        fail=1; continue
    fi

    jit_total=$(best_ms "$HCC" -O3 "$hc")
    jit_load=$(best_ms "$HCC" --no-run -O3 "$hc")
    jit_run=$(awk "BEGIN { r = $jit_total - $jit_load; if (r < 0) r = 0; printf \"%.1f\", r }")
    aot=$(best_ms "$TMP/$name.hcc")
    ccm=$(best_ms "$TMP/$name.cc")
    ratio=$(awk "BEGIN { c=$ccm; if (c == 0) c = 0.1; printf \"%.2f\", $aot / c }")
    printf '%-10s %13s %12s %12s %12s %10s\n' "$name" "$jit_load" "$jit_run" "$aot" "$ccm" "$ratio"
done

if [ "$fail" -ne 0 ]; then
    echo; echo "benchmark FAILURES above (compile or checksum)"; exit 1
fi
echo
echo "checksums matched for every pair (best of $REPS, sequential)"
echo "jit-load = hcc --no-run (startup+compile+materialize); jit-run = total - load"
