#!/bin/sh
# hlang test runner: golden tests under both JIT and AOT, plus error tests.
# Usage: HCC=path/to/hcc tests/run_tests.sh [name-filter]
set -u

HCC="${HCC:-build/hcc}"
FILTER="${1:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }

check() { # name mode expected actual rc
    name=$1; mode=$2; expected=$3; actual=$4; rc=$5
    if [ "$rc" -ne 0 ]; then
        red "FAIL $name [$mode] (exit $rc)"
        sed 's/^/    /' "$actual"
        fail=$((fail+1))
        return
    fi
    if ! diff -u "$expected" "$actual" > "$TMP/diff" 2>&1; then
        red "FAIL $name [$mode]"
        sed 's/^/    /' "$TMP/diff" | head -30
        fail=$((fail+1))
        return
    fi
    pass=$((pass+1))
}

# ------------------------------------------------------------ golden tests
for hc in "$HERE"/cases/*.HC; do
    name=$(basename "$hc" .HC)
    case "$name" in
        *"$FILTER"*) ;;
        *) continue ;;
    esac
    base="$HERE/cases/$name"
    stdin_f="/dev/null"
    [ -f "$base.in" ] && stdin_f="$base.in"

    # JIT
    exp="$base.out"
    [ -f "$base.jit.out" ] && exp="$base.jit.out"
    if [ -f "$exp" ]; then
        "$HCC" "$hc" -- test-arg1 test-arg2 < "$stdin_f" > "$TMP/out" 2>&1
        check "$name" jit "$exp" "$TMP/out" $?
    fi

    # AOT
    exp="$base.out"
    [ -f "$base.aot.out" ] && exp="$base.aot.out"
    if [ -f "$exp" ]; then
        if ! "$HCC" "$hc" -o "$TMP/prog" > "$TMP/out" 2>&1; then
            red "FAIL $name [aot-compile]"
            sed 's/^/    /' "$TMP/out"
            fail=$((fail+1))
        else
            "$TMP/prog" test-arg1 test-arg2 < "$stdin_f" > "$TMP/out" 2>&1
            check "$name" aot "$exp" "$TMP/out" $?
        fi
    fi
done

# ------------------------------------------------------------ error tests
for hc in "$HERE"/errors/*.HC; do
    [ -e "$hc" ] || continue
    name=$(basename "$hc" .HC)
    case "$name" in
        *"$FILTER"*) ;;
        *) continue ;;
    esac
    want=$(sed -n 's|^//ERR: ||p' "$hc")
    if "$HCC" "$hc" > "$TMP/out" 2>&1; then
        red "FAIL $name [error-test]: compiled but should not"
        fail=$((fail+1))
        continue
    fi
    if [ -n "$want" ] && ! grep -qF "$want" "$TMP/out"; then
        red "FAIL $name [error-test]: message mismatch"
        printf '    wanted: %s\n' "$want"
        sed 's/^/    got: /' "$TMP/out" | head -5
        fail=$((fail+1))
        continue
    fi
    pass=$((pass+1))
done

# ------------------------------------------------------------ front end dumps
for hc in "$HERE"/frontend/*.HC; do
    [ -e "$hc" ] || continue
    name=$(basename "$hc" .HC)
    case "$name" in
        *"$FILTER"*) ;;
        *) continue ;;
    esac
    base="$HERE/frontend/$name"
    if [ -f "$base.tokens" ]; then
        "$HCC" --dump-tokens "$hc" > "$TMP/out" 2>&1
        check "$name" tokens "$base.tokens" "$TMP/out" $?
    fi
    if [ -f "$base.ast" ]; then
        "$HCC" --dump-ast "$hc" > "$TMP/out" 2>&1
        check "$name" ast "$base.ast" "$TMP/out" $?
    fi
done

echo
if [ "$fail" -eq 0 ]; then
    green "All $pass tests passed."
    exit 0
else
    red "$fail failed, $pass passed."
    exit 1
fi
