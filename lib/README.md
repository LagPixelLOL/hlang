# The hlang stdlib (`HolyC.HH` + `runtime/hcrt.c`)

Minimal but faithful to TempleOS names and behaviors. `lib/HolyC.HH` is
the **prelude**: hcc auto-includes it before your program (suppress
with `--no-prelude`). Runtime symbols live in `hcrt` under `HC_*`
names; the prelude binds them to their HolyC names with `_extern` —
exactly the mechanism TempleOS uses to bind C to asm. Where a routine
is expressible in plain HolyC (`ToUpper`, `StrOcc`, ...), it is written
in HolyC in the prelude: the stdlib eats its own dog food.

Policy (see DEVELOPMENT.md §3): TempleOS *names and feel*, hosted
*implementation*. Add a function only when a real program or test needs
it, always with the TempleOS name and signature. Every function below
is exercised by the test suite (71/71 — which also proves each `HC_*`
symbol is registered in both the JIT symbol map and `libhcrt.a`).

## Defines & constants

| group | names |
|---|---|
| truth | `TRUE FALSE ON OFF NULL`, `Bool` (= `U8`) — `#define`s, as in TempleOS |
| int bounds | `I8_MIN I8_MAX U8_MAX I16_MIN I16_MAX U16_MAX I32_MIN I32_MAX U32_MAX I64_MIN I64_MAX U64_MAX` |
| float consts | `pi log2_10 log10_2 loge_2` (F64 — TempleOS's 10-byte versions are 64-bit here, see DEVIATIONS.md) |
| compiler options | `OPTf_*` (12 flags) + `Option(opt_num,val)` — accepted no-ops |

## The task: `Fs`

```holyc
class CTask
{
  I64 except_ch;          // throw()'s char code, readable in catch{}
  I64 catch_except;       // set TRUE in catch{} to stop the handler search
  I64 except_callers[8];  // return addrs at throw()
  U8  *task_name;
};
_extern HC_Fs CTask *Fs;
```

`Fs` carries only the exception-related fields — not the full TempleOS
task struct, and there is no `Fs->hash_table` (DEVIATIONS.md).

## Output

| signature | notes |
|---|---|
| `U0 Print(U8 *fmt,...)` | the fmt-code engine; bare string/`"fmt",args` statements compile to it |
| `U0 PutChars(I64 chars)` | up to 8 packed chars (`'Hello '`); `'c';` and `'' expr;` compile to it |
| `U0 PutChar(I64 ch)` | one byte, raw |
| `U0 PutS(U8 *st)` | raw string — **no** fmt interpretation (`%` is literal) |
| `U8 *StrPrint(U8 *dst,U8 *fmt,...)` | sprintf into `dst` (MAllocs if `dst==NULL`) |
| `U8 *MStrPrint(U8 *fmt,...)` | sprintf into a fresh MAlloc'ed string |
| `U8 *CatPrint(U8 *dst,U8 *fmt,...)` | append formatted text to `dst` |
| `U8 *StrPrintJoin(U8 *dst,U8 *fmt,I64 argc,I64 *argv)` | vararg forwarding, TempleOS style (`Fmt(...)` wrappers) |

### `Print` format codes

`%d %u %x %X` (int, dec/hex) · `%c` (packed chars, so `'AB'` prints
`AB`) · `%s` · `%q`/`%Q` (quoted/escaped) · `%f %e %g` (F64) ·
`%p`/`%P` (pointer, hex) · `%z` (index into a `\0`-separated list) ·
`%D %T` (CDate date/time) · `%%` literal. Flags `-` `0` `+` `,`
(digit grouping), width and `.precision`.

Deviations: `%3c` repeat counts and DolDoc-specific codes are not
implemented; `%P` prints hex without symbol lookup; unknown codes pass
through literally; `%u`/lowercase-`%x` may be hosted supersets
(Print.DD is not in the doc archive). `$` is the DolDoc escape: `$$`
prints one `$`.

## Input

| signature | notes |
|---|---|
| `U8 *GetStr(U8 *msg=NULL)` | prompt + read line, MAlloc'ed (caller Frees) |
| `I64 GetChar()` | one byte from stdin (−1 on EOF), no echo |
| `I64 GetI64(U8 *msg=NULL,I64 dft=0,I64 min=I64_MIN,I64 max=I64_MAX)` | reprompts until in range; empty line → `dft` |
| `F64 GetF64(U8 *msg=NULL,F64 dft=0.0)` | empty line → `dft` |
| `Bool YorN()` | y/n prompt |
| `U0 PressAKey()` | consumes one key (byte) |

## Memory

| signature | notes |
|---|---|
| `U0 *MAlloc(I64 size)` | heap alloc; aborts on OOM like TempleOS panics |
| `U0 *CAlloc(I64 size)` | zeroed alloc |
| `U0 *MAllocIdent(U0 *src)` | duplicate an allocation (uses `MSize`) |
| `U0 Free(U0 *addr)` | **`Free(NULL)` is legal** — Terry said so |
| `I64 MSize(U0 *src)` | true size of the allocation. Deviation: hcrt does not round sizes up like the TempleOS heap, so this equals the requested size |
| `U0 *MemCpy(U0 *dst,U0 *src,I64 cnt)` / `U0 *MemSet(U0 *dst,I64 val,I64 cnt)` / `I64 MemCmp(U0 *ptr1,U0 *ptr2,I64 cnt)` | byte ops |

## Strings

`I64 StrLen(U8 *st)` · `U8 *StrCpy(U8 *dst,U8 *src)` · `U8 *StrNew(U8
*st)` (MAlloc'ed copy) · `I64 StrCmp(U8 *st1,U8 *st2)` · `I64
StrNCmp(U8 *st1,U8 *st2,I64 n)` · `I64 StrICmp(U8 *st1,U8 *st2)` ·
`U8 *StrFind(U8 *needle,U8 *haystack)` (NULL if absent) · `I64
Str2I64(U8 *st,I64 radix=10)` · `F64 Str2F64(U8 *st)`.

Written in HolyC in the prelude itself: `U8 ToUpper(U8 ch)` · `U8
ToLower(U8 ch)` · `I64 StrOcc(U8 *src,I64 ch)` · `U0 SwapI64(I64
*num1,I64 *num2)` · `I64 ClampI64(I64 num,I64 lo,I64 hi)`.

## Math

F64: `Abs Sqrt Sin Cos Tan ASin ACos ATan Ln Log2 Log10 Exp Floor Ceil
Round Trunc` (one arg), `Arg(x,y)` (angle of the vector — note the
TempleOS argument order), `Pow(base,e)`. The `` ` `` operator is the
idiomatic power spelling.

I64: `AbsI64 SignI64 SqrI64` · `MinI64(n1,n2)` / `MaxI64(n1,n2)`.

## Random & bits

`U0 Seed(I64 seed=0)` — `0` (or bare `Seed;`) seeds from the clock;
any other value gives a reproducible stream. `U64 RandU64()` · `I64
RandI64()` · `U32 RandU32()` · `U16 RandU16()`.

Bit routines wrap x86 `BT`-family semantics on any buffer: `Bool
Bt(U0 *bit_field,I64 bit_num)` (test) · `Bts` (test-and-set) · `Btr`
(test-and-reset) · `Btc` (test-and-complement) · `I64 BCnt(I64 d)`
(popcount).

## Time, system, files

| signature | notes |
|---|---|
| `F64 tS()` | seconds since program start |
| `I64 Now()` | CDate: `days_since_1/1/0 << 32 \| day_fraction` |
| `U0 Sleep(I64 mS)` | millisecond sleep |
| `U0 Exit(I64 code=0)` | flush + exit |
| `U0 Call(U0 *addr)` | call a code address (fn ptrs are also callable directly) |
| `I64 ArgCnt()` / `U8 *ArgStr(I64 i)` | host command-line args — **hosted extras**, not TempleOS names |
| `U0 *FileRead(U8 *filename,I64 *size=NULL)` | whole file, MAlloc'ed, NUL-terminated for convenience; NULL + size 0 if missing |
| `I64 FileWrite(U8 *filename,U0 *fbuf,I64 size)` | returns bytes written |

## Exceptions

`U0 throw(I64 ch=0)` — "throw is a function with an 8-byte or less
char arg" (`throw('Point1')`). The handler search continues **outward**
unless the `catch{}` sets `Fs->catch_except=TRUE`. `U0 PutExcept(Bool
catch_it=TRUE)` prints `Except:'CH' at <caller>` and (by default)
claims the exception. Lowered with setjmp frames in hcrt — see
DEVIATIONS.md (don't return out of a `try{}`).

## `#exe {}` support

`U0 StreamPrint(U8 *fmt,...)` — inside `#exe {}` blocks, splices
formatted text into the token stream after the block. (TempleOS's
`Cd()`/`StreamDir` are not provided; `__DIR__` etc. are builtins.)

## Adding a function

Checklist in DEVELOPMENT.md: implement `HC_Foo` in `runtime/hcrt.c`,
register it in `defineRuntimeSymbols()` (`src/jit.cpp`), bind it here
with `_extern HC_Foo ... Foo(...)`, and land a golden test in
`tests/cases/` in the same commit — the suite runs it under JIT and
AOT, which catches a missing JIT-map entry immediately.
