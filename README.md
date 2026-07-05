# hlang — a HolyC compiler (LLVM front end)

`hcc` is an LLVM-based compiler for Terry A. Davis' HolyC language, supporting
both JIT execution (the TempleOS way) and AOT compilation to native
executables/objects, plus a minimal stdlib in the spirit of the TempleOS
kernel/runtime library.

Status: under construction.

## Layout

- `src/`      — the compiler (C++17, LLVM 21)
- `runtime/`  — the C runtime that backs the HolyC stdlib
- `lib/`      — HolyC prelude/stdlib headers (auto-included, KernelA.HH spirit)
- `tests/`    — golden tests run under both JIT and AOT

## Build

```sh
cmake -S . -B build -G Ninja
ninja -C build
ninja -C build check   # run the test suite
```
