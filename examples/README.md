# examples/

Small, self-contained HolyC programs with commentary. Each runs under
JIT and AOT:

```sh
build/hcc examples/Mandelbrot.HC              # JIT: compile + run now
build/hcc examples/Mandelbrot.HC -o mandel    # AOT: native executable
```

| file             | what it shows off                                             |
|------------------|---------------------------------------------------------------|
| `HelloTemple.HC` | print sugar: bare strings, `''`, packed char consts, `%z`, `$$` |
| `FizzBuzz.HC`    | no `continue` (goto!), jump-table `switch`, stacked cases, `%z` as data |
| `Mandelbrot.HC`  | F64 math, `#define`, `''` with computed characters            |
| `Primes.HC`      | `CAlloc`/`MSize`/`Free`, `Bt`/`Bts`/`BCnt` bit routines, "No type-checking" |
| `Dungeon.HC`     | packed classes, single inheritance, fn-ptr members, postfix casts, default/skipped args, `sub_switch` |
| `Oracle.HC`      | `try`/`catch`/`throw('Char8')`, `Fs->except_ch`, the outward handler search |
| `CompileTime.HC` | `#exe{}` + `StreamPrint`: tables, functions and strings written by the compiler at build time |

They are written in TempleOS style (2-space indent, terse) and lean on
the ways HolyC is *not* C -- read the comments.
