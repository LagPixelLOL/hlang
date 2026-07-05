#!/bin/sh
# Run ONE hlang test in isolation. Used by ctest (one process per test, so
# `ctest -j` is safe) and by run_tests.sh (sequential fallback).
#
#   run_one.sh <mode> <file.HC>
#   mode: jit | aot | error | tokens | ast
#
# Exit: 0 pass, 1 fail, 77 skip.
#   jit/aot/error: a missing golden is a FAILURE (catches deleted/renamed
#                  goldens -- every case must have its output committed).
#   tokens/ast:    a missing golden is a SKIP -- each frontend file targets
#                  one dump mode on purpose; ctest only registers the mode
#                  whose golden exists, so under ctest nothing is skipped.
# Env:  HCC = path to hcc (required)
set -u

mode=$1
hc=$2
base=${hc%.HC}
HCC="${HCC:-build/hcc}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fail() { echo "FAIL [$mode] $hc"; [ -n "${1:-}" ] && sed 's/^/    /' "$1"; exit 1; }

case "$mode" in
    jit)
        exp="$base.out"; [ -f "$base.jit.out" ] && exp="$base.jit.out"
        [ -f "$exp" ] || { echo "FAIL [jit] $hc: no golden ($exp)"; exit 1; }
        stdin_f=/dev/null; [ -f "$base.in" ] && stdin_f="$base.in"
        "$HCC" "$hc" -- test-arg1 test-arg2 < "$stdin_f" > "$TMP/out" 2>&1
        diff -u "$exp" "$TMP/out" > "$TMP/diff" 2>&1 || fail "$TMP/diff"
        ;;
    aot)
        exp="$base.out"; [ -f "$base.aot.out" ] && exp="$base.aot.out"
        [ -f "$exp" ] || { echo "FAIL [aot] $hc: no golden ($exp)"; exit 1; }
        stdin_f=/dev/null; [ -f "$base.in" ] && stdin_f="$base.in"
        "$HCC" "$hc" -o "$TMP/prog" > "$TMP/cerr" 2>&1 || fail "$TMP/cerr"
        "$TMP/prog" test-arg1 test-arg2 < "$stdin_f" > "$TMP/out" 2>&1
        diff -u "$exp" "$TMP/out" > "$TMP/diff" 2>&1 || fail "$TMP/diff"
        ;;
    error)
        want=$(sed -n 's|^//ERR: ||p' "$hc")
        if "$HCC" "$hc" > "$TMP/out" 2>&1; then
            echo "FAIL [error] $hc: compiled but should not"; exit 1
        fi
        if [ -n "$want" ] && ! grep -qF "$want" "$TMP/out"; then
            echo "FAIL [error] $hc: message mismatch"
            printf '    wanted: %s\n' "$want"
            sed 's/^/    got: /' "$TMP/out" | head -5
            exit 1
        fi
        ;;
    tokens)
        [ -f "$base.tokens" ] || exit 77
        "$HCC" --dump-tokens "$hc" > "$TMP/out" 2>&1
        diff -u "$base.tokens" "$TMP/out" > "$TMP/diff" 2>&1 || fail "$TMP/diff"
        ;;
    ast)
        [ -f "$base.ast" ] || exit 77
        "$HCC" --dump-ast "$hc" > "$TMP/out" 2>&1
        diff -u "$base.ast" "$TMP/out" > "$TMP/diff" 2>&1 || fail "$TMP/diff"
        ;;
    *)
        echo "run_one.sh: unknown mode '$mode'" >&2; exit 2
        ;;
esac
exit 0
