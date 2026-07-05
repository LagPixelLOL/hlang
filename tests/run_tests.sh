#!/bin/sh
# Sequential test runner (cmake-less usage). ctest is the primary path:
#   ninja -C build check      # parallel via ctest
# This script shares run_one.sh with ctest so behavior can't drift.
#
#   HCC=build/hcc tests/run_tests.sh [name-filter]
set -u

HCC="${HCC:-build/hcc}"
export HCC
FILTER="${1:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"

pass=0 fail=0 skip=0
run() { # mode file
    case "$2" in *"$FILTER"*) ;; *) return ;; esac
    sh "$HERE/run_one.sh" "$1" "$2"
    case $? in
        0) pass=$((pass+1)) ;;
        77) skip=$((skip+1)) ;;
        *) fail=$((fail+1)) ;;
    esac
}

for hc in "$HERE"/cases/*.HC;    do run jit "$hc"; run aot "$hc"; done
for hc in "$HERE"/errors/*.HC;   do [ -e "$hc" ] && run error "$hc"; done
for hc in "$HERE"/frontend/*.HC; do [ -e "$hc" ] && { run tokens "$hc"; run ast "$hc"; }; done

echo
if [ "$fail" -eq 0 ]; then
    printf '\033[32mAll %d tests passed (%d skipped).\033[0m\n' "$pass" "$skip"
    exit 0
else
    printf '\033[31m%d failed, %d passed, %d skipped.\033[0m\n' "$fail" "$pass" "$skip"
    exit 1
fi
