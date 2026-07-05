/* hlang -- HolyC runtime implementation. Self-contained: libc + libm only. */
#include "hcrt.h"

#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ================================================================= task */

static HCTask hc_main_task = {0, 0, {0}, (char*)"Adam"};
HCTask* HC_Fs = &hc_main_task;

static int64_t hc_argc;
static char** hc_argv;
static double hc_t0;

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

void HC_RtInit(int64_t argc, char** argv) {
    hc_argc = argc;
    hc_argv = argv;
    hc_t0 = now_seconds();
}

int64_t HC_ArgCnt(void) { return hc_argc; }
char* HC_ArgStr(int64_t i) { return (i >= 0 && i < hc_argc) ? hc_argv[i] : (char*)""; }

/* ================================================================ memory */

typedef struct {
    uint64_t size;
    uint64_t magic;
} HCMemHdr;
#define HC_MEM_MAGIC 0x686F6C79684D454DULL

void* HC_MAlloc(int64_t size) {
    if (size < 0) size = 0;
    HCMemHdr* h = (HCMemHdr*)malloc(sizeof(HCMemHdr) + (size_t)size);
    if (!h) {
        fprintf(stderr, "hcrt: out of memory (MAlloc %lld)\n", (long long)size);
        exit(1);
    }
    h->size = (uint64_t)size;
    h->magic = HC_MEM_MAGIC;
    return h + 1;
}

void* HC_CAlloc(int64_t size) {
    void* p = HC_MAlloc(size);
    memset(p, 0, (size_t)size);
    return p;
}

int64_t HC_MSize(void* p) {
    if (!p) return 0;
    HCMemHdr* h = (HCMemHdr*)p - 1;
    return h->magic == HC_MEM_MAGIC ? (int64_t)h->size : 0;
}

void* HC_MAllocIdent(void* src) {
    if (!src) return NULL;
    int64_t n = HC_MSize(src);
    void* p = HC_MAlloc(n);
    memcpy(p, src, (size_t)n);
    return p;
}

/* You CAN Free() a NULL ptr. */
void HC_Free(void* p) {
    if (!p) return;
    HCMemHdr* h = (HCMemHdr*)p - 1;
    if (h->magic != HC_MEM_MAGIC) {
        fprintf(stderr, "hcrt: Free() of ptr not from MAlloc()\n");
        exit(1);
    }
    h->magic = 0;
    free(h);
}

void* HC_MemCpy(void* dst, void* src, int64_t n) { return memcpy(dst, src, (size_t)n); }
void* HC_MemSet(void* dst, int64_t val, int64_t n) { return memset(dst, (int)val, (size_t)n); }
int64_t HC_MemCmp(void* a, void* b, int64_t n) { return memcmp(a, b, (size_t)n); }

/* =============================================================== strings */

int64_t HC_StrLen(char* s) { return s ? (int64_t)strlen(s) : 0; }
char* HC_StrCpy(char* dst, char* src) { return strcpy(dst, src ? src : ""); }
int64_t HC_StrCmp(char* a, char* b) { return strcmp(a ? a : "", b ? b : ""); }
int64_t HC_StrNCmp(char* a, char* b, int64_t n) {
    return strncmp(a ? a : "", b ? b : "", (size_t)n);
}
char* HC_StrFind(char* needle, char* haystack) {
    if (!needle || !haystack) return NULL;
    return strstr(haystack, needle);
}
int64_t HC_StrICmp(char* a, char* b) { return strcasecmp(a ? a : "", b ? b : ""); }

char* HC_StrNew(char* s) {
    if (!s) s = (char*)"";
    int64_t n = (int64_t)strlen(s) + 1;
    char* p = (char*)HC_MAlloc(n);
    memcpy(p, s, (size_t)n);
    return p;
}

int64_t HC_Str2I64(char* s, int64_t radix) { return s ? (int64_t)strtoll(s, NULL, (int)radix) : 0; }
double HC_Str2F64(char* s) { return s ? strtod(s, NULL) : 0.0; }

/* ============================================================ formatting */
/* TempleOS Print() fmt codes: %d %u %x %X %c %s %f %e %g %p/%P %D %T %z %Q
 * flags: '-' left justify, '0' zero pad, ',' thousand grouping.          */

typedef struct {
    char* buf;
    size_t len, cap;
    int to_stdout;
} HCOut;

static void out_emit(HCOut* o, const char* s, size_t n) {
    if (o->to_stdout) {
        fwrite(s, 1, n, stdout);
        return;
    }
    if (o->len + n + 1 > o->cap) {
        size_t nc = o->cap ? o->cap : 64;
        while (o->len + n + 1 > nc) nc *= 2;
        o->buf = (char*)realloc(o->buf, nc);
        o->cap = nc;
    }
    memcpy(o->buf + o->len, s, n);
    o->len += n;
    o->buf[o->len] = 0;
}

static void out_pad(HCOut* o, char c, int64_t n) {
    while (n-- > 0) out_emit(o, &c, 1);
}

/* emit an integer body with optional comma grouping into tmp; returns len */
static int int_body(char* tmp, uint64_t v, int base, int upper, int commas, int neg) {
    char digits[64];
    int n = 0;
    const char* hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (v == 0) digits[n++] = '0';
    while (v) {
        digits[n++] = hex[v % (uint64_t)base];
        v /= (uint64_t)base;
    }
    int len = 0;
    if (neg) tmp[len++] = '-';
    for (int i = n - 1; i >= 0; i--) {
        tmp[len++] = digits[i];
        if (commas && base == 10 && i > 0 && i % 3 == 0) tmp[len++] = ',';
    }
    return len;
}

static void fmt_field(HCOut* o, const char* body, int len, int width, int left, int zero) {
    if (len >= width) {
        out_emit(o, body, (size_t)len);
        return;
    }
    if (left) {
        out_emit(o, body, (size_t)len);
        out_pad(o, ' ', width - len);
    } else if (zero) {
        /* keep sign before zeros */
        int i = 0;
        if (len && (body[0] == '-' || body[0] == '+')) {
            out_emit(o, body, 1);
            i = 1;
        }
        out_pad(o, '0', width - len);
        out_emit(o, body + i, (size_t)(len - i));
    } else {
        out_pad(o, ' ', width - len);
        out_emit(o, body, (size_t)len);
    }
}

static void emit_packed_chars(HCOut* o, int64_t v) {
    /* up to 8 chars, LSB first, zero bytes skipped -- PutChars() semantics */
    for (int i = 0; i < 8; i++) {
        char c = (char)((uint64_t)v >> (8 * i));
        if (c) out_emit(o, &c, 1);
    }
}

static void fmt_date(HCOut* o, int64_t cdate, int time_part) {
    /* CDate: days since 1/1/0 in upper 32 bits, fraction of day in lower */
    int64_t days = cdate >> 32;
    uint32_t frac = (uint32_t)(cdate & 0xFFFFFFFF);
    int64_t secs = (int64_t)((double)frac * 86400.0 / 4294967296.0);
    char tmp[64];
    if (time_part) {
        snprintf(tmp, sizeof tmp, "%02lld:%02lld:%02lld", (long long)(secs / 3600),
                 (long long)(secs / 60 % 60), (long long)(secs % 60));
        out_emit(o, tmp, strlen(tmp));
        return;
    }
    /* civil-from-days, epoch shifted to year 0 (proleptic Gregorian) */
    int64_t z = days - 719468 + 719528; /* days since 1970 shifted... */
    z = days - 719528;                  /* days since 1970-01-01 */
    z += 719468;                        /* days since 0000-03-01 */
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint64_t doe = (uint64_t)(z - era * 146097);
    uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = (int64_t)yoe + era * 400;
    uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint64_t mp = (5 * doy + 2) / 153;
    uint64_t d = doy - (153 * mp + 2) / 5 + 1;
    uint64_t m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y++;
    snprintf(tmp, sizeof tmp, "%02llu/%02llu/%04lld", (unsigned long long)m, (unsigned long long)d,
             (long long)y);
    out_emit(o, tmp, strlen(tmp));
}

static void hc_format(HCOut* o, const char* fmt, int64_t argc, const int64_t* argv) {
    int64_t ai = 0;
    if (!fmt) return;
#define NEXT_ARG (ai < argc ? argv[ai++] : 0)
    for (const char* p = fmt; *p; p++) {
        if (*p != '%') {
            out_emit(o, p, 1);
            continue;
        }
        p++;
        if (!*p) break;
        if (*p == '%') {
            out_emit(o, "%", 1);
            continue;
        }
        int left = 0, zero = 0, commas = 0, plus = 0;
        for (;; p++) {
            if (*p == '-')
                left = 1;
            else if (*p == '0')
                zero = 1;
            else if (*p == ',')
                commas = 1;
            else if (*p == '+')
                plus = 1;
            else
                break;
        }
        int width = 0, prec = -1;
        while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
        if (*p == '.') {
            p++;
            prec = 0;
            while (*p >= '0' && *p <= '9') prec = prec * 10 + (*p++ - '0');
        }
        char tmp[512];
        int len;
        switch (*p) {
            case 'd':
            case 'i': {
                int64_t v = NEXT_ARG;
                int neg = v < 0;
                uint64_t uv = neg ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
                len = int_body(tmp, uv, 10, 0, commas, neg);
                if (plus && !neg) {
                    memmove(tmp + 1, tmp, (size_t)len++);
                    tmp[0] = '+';
                }
                fmt_field(o, tmp, len, width, left, zero);
                break;
            }
            case 'u': {
                uint64_t v = (uint64_t)NEXT_ARG;
                len = int_body(tmp, v, 10, 0, commas, 0);
                fmt_field(o, tmp, len, width, left, zero);
                break;
            }
            case 'x':
            case 'X': {
                uint64_t v = (uint64_t)NEXT_ARG;
                len = int_body(tmp, v, 16, *p == 'X', 0, 0);
                fmt_field(o, tmp, len, width, left, zero);
                break;
            }
            case 'p':
            case 'P': {
                uint64_t v = (uint64_t)NEXT_ARG;
                len = snprintf(tmp, sizeof tmp, "%016llX", (unsigned long long)v);
                fmt_field(o, tmp, len, width, left, zero);
                break;
            }
            case 'c': {
                HCOut sub = {0};
                emit_packed_chars(&sub, NEXT_ARG);
                fmt_field(o, sub.buf ? sub.buf : "", (int)sub.len, width, left, 0);
                free(sub.buf);
                break;
            }
            case 's': {
                char* s = (char*)NEXT_ARG;
                if (!s) s = (char*)"(null)";
                len = (int)strlen(s);
                if (prec >= 0 && len > prec) len = prec;
                fmt_field(o, s, len, width, left, 0);
                break;
            }
            case 'q':
            case 'Q': { /* quoted/escaped string */
                char* s = (char*)NEXT_ARG;
                if (!s) s = (char*)"";
                HCOut sub = {0};
                for (; *s; s++) {
                    if (*s == '"' || *s == '\\') {
                        out_emit(&sub, "\\", 1);
                        out_emit(&sub, s, 1);
                    } else if (*s == '\n') {
                        out_emit(&sub, "\\n", 2);
                    } else {
                        out_emit(&sub, s, 1);
                    }
                }
                fmt_field(o, sub.buf ? sub.buf : "", (int)sub.len, width, left, 0);
                free(sub.buf);
                break;
            }
            case 'f':
            case 'e':
            case 'g': {
                int64_t raw = NEXT_ARG;
                double d;
                memcpy(&d, &raw, 8);
                char ff[16];
                snprintf(ff, sizeof ff, "%%%s.*%c", plus ? "+" : "", *p);
                len = snprintf(tmp, sizeof tmp, ff, prec < 0 ? 6 : prec, d);
                fmt_field(o, tmp, len, width, left, zero);
                break;
            }
            case 'z': { /* "%z",idx,"Zero\0One\0Two\0" -- indexed \0 list */
                int64_t idx = NEXT_ARG;
                char* lst = (char*)NEXT_ARG;
                if (lst) {
                    while (idx-- > 0 && *lst) lst += strlen(lst) + 1;
                    len = (int)strlen(lst);
                    fmt_field(o, lst, len, width, left, 0);
                }
                break;
            }
            case 'D':
            case 'T': {
                int64_t v = NEXT_ARG;
                fmt_date(o, v, *p == 'T');
                break;
            }
            default: { /* unknown code: emit verbatim */
                out_emit(o, "%", 1);
                out_emit(o, p, 1);
                break;
            }
        }
    }
#undef NEXT_ARG
}

void HC_Print(char* fmt, int64_t argc, int64_t* argv) {
    HCOut o = {0};
    o.to_stdout = 1;
    hc_format(&o, fmt, argc, argv);
    fflush(stdout);
}

void HC_PutChars(int64_t chars) {
    HCOut o = {0};
    o.to_stdout = 1;
    emit_packed_chars(&o, chars);
    fflush(stdout);
}

void HC_PutChar(int64_t ch) {
    fputc((int)(ch & 0xFF), stdout);
    fflush(stdout);
}

void HC_PutS(char* st) {
    if (st) fputs(st, stdout);
    fflush(stdout);
}

char* HC_StrPrint(char* dst, char* fmt, int64_t argc, int64_t* argv) {
    HCOut o = {0};
    hc_format(&o, fmt, argc, argv);
    if (!dst) { /* TempleOS wants a dst; be forgiving and MAlloc one */
        char* r = HC_StrNew(o.buf ? o.buf : (char*)"");
        free(o.buf);
        return r;
    }
    strcpy(dst, o.buf ? o.buf : "");
    free(o.buf);
    return dst;
}

char* HC_MStrPrint(char* fmt, int64_t argc, int64_t* argv) {
    HCOut o = {0};
    hc_format(&o, fmt, argc, argv);
    char* r = HC_StrNew(o.buf ? o.buf : (char*)"");
    free(o.buf);
    return r;
}

char* HC_CatPrint(char* dst, char* fmt, int64_t argc, int64_t* argv) {
    HCOut o = {0};
    hc_format(&o, fmt, argc, argv);
    if (dst) strcat(dst, o.buf ? o.buf : "");
    free(o.buf);
    return dst;
}

/* StrPrintJoin(NULL,fmt,argc,argv): SPrintF() with MAlloc()ed string. */
char* HC_StrPrintJoin(char* dst, char* fmt, int64_t argc, int64_t* argv) {
    HCOut o = {0};
    hc_format(&o, fmt, argc, argv);
    const char* add = o.buf ? o.buf : "";
    if (!dst) {
        char* r = HC_StrNew((char*)add);
        free(o.buf);
        return r;
    }
    int64_t dl = HC_StrLen(dst);
    char* r = (char*)HC_MAlloc(dl + (int64_t)strlen(add) + 1);
    memcpy(r, dst, (size_t)dl);
    strcpy(r + dl, add);
    HC_Free(dst);
    free(o.buf);
    return r;
}

/* ================================================================= input */

char* HC_GetStr(char* msg) {
    if (msg) {
        fputs(msg, stdout);
        fflush(stdout);
    }
    char* line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) {
        free(line);
        return HC_StrNew((char*)"");
    }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
    char* r = HC_StrNew(line);
    free(line);
    return r;
}

int64_t HC_GetChar(void) {
    int c = fgetc(stdin);
    return c < 0 ? -1 : c;
}

int64_t HC_GetI64(char* msg, int64_t dft, int64_t min, int64_t max) {
    for (;;) {
        char* s = HC_GetStr(msg);
        if (!s[0]) {
            HC_Free(s);
            return dft;
        }
        char* end;
        long long v = strtoll(s, &end, 0);
        int ok = end != s;
        HC_Free(s);
        if (ok && v >= min && v <= max) return v;
        if (feof(stdin)) return dft;
    }
}

double HC_GetF64(char* msg, double dft) {
    char* s = HC_GetStr(msg);
    if (!s[0]) {
        HC_Free(s);
        return dft;
    }
    double v = strtod(s, NULL);
    HC_Free(s);
    return v;
}

int64_t HC_YorN(void) {
    for (;;) {
        char* s = HC_GetStr(NULL);
        char c = s[0];
        HC_Free(s);
        if (c == 'y' || c == 'Y') return 1;
        if (c == 'n' || c == 'N') return 0;
        if (feof(stdin)) return 0;
    }
}

void HC_PressAKey(void) { HC_GetChar(); }

/* ================================================================== math */

double HC_Abs(double x) { return fabs(x); }
double HC_Sqrt(double x) { return sqrt(x); }
double HC_Sin(double x) { return sin(x); }
double HC_Cos(double x) { return cos(x); }
double HC_Tan(double x) { return tan(x); }
double HC_ASin(double x) { return asin(x); }
double HC_ACos(double x) { return acos(x); }
double HC_ATan(double x) { return atan(x); }
double HC_Arg(double x, double y) { return atan2(y, x); }
double HC_Ln(double x) { return log(x); }
double HC_Log2(double x) { return log2(x); }
double HC_Log10(double x) { return log10(x); }
double HC_Exp(double x) { return exp(x); }
double HC_Pow(double base, double e) { return pow(base, e); }
/* integer form of HolyC's backtick power operator */
int64_t HC_PowI64(int64_t base, int64_t e) {
    if (e < 0) return 0;
    int64_t r = 1;
    while (e) {
        if (e & 1) r *= base;
        base *= base;
        e >>= 1;
    }
    return r;
}
double HC_Floor(double x) { return floor(x); }
double HC_Ceil(double x) { return ceil(x); }
double HC_Round(double x) { return round(x); }
double HC_Trunc(double x) { return trunc(x); }
int64_t HC_AbsI64(int64_t x) { return x < 0 ? -x : x; }
int64_t HC_MinI64(int64_t a, int64_t b) { return a < b ? a : b; }
int64_t HC_MaxI64(int64_t a, int64_t b) { return a > b ? a : b; }
int64_t HC_SignI64(int64_t x) { return x < 0 ? -1 : x > 0 ? 1 : 0; }
int64_t HC_SqrI64(int64_t x) { return x * x; }

/* ================================================================ random */

static uint64_t hc_rand_state = 0x5EED5EED5EED5EEDULL;

void HC_Seed(int64_t seed) {
    if (!seed) seed = (int64_t)time(NULL) ^ ((int64_t)clock() << 20);
    hc_rand_state = (uint64_t)seed ? (uint64_t)seed : 1;
}

int64_t HC_RandU64(void) { /* xorshift64* */
    uint64_t x = hc_rand_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    hc_rand_state = x;
    return (int64_t)(x * 0x2545F4914F6CDD1DULL);
}
int64_t HC_RandI64(void) { return HC_RandU64(); }
int64_t HC_RandU32(void) { return (int64_t)((uint64_t)HC_RandU64() >> 32); }
int64_t HC_RandU16(void) { return (int64_t)((uint64_t)HC_RandU64() >> 48); }

/* ================================================================== bits */

int64_t HC_Bt(void* bit_field, int64_t bit_num) {
    uint8_t* p = (uint8_t*)bit_field;
    return (p[bit_num >> 3] >> (bit_num & 7)) & 1;
}
int64_t HC_Bts(void* bit_field, int64_t bit_num) {
    uint8_t* p = (uint8_t*)bit_field;
    int64_t old = (p[bit_num >> 3] >> (bit_num & 7)) & 1;
    p[bit_num >> 3] |= (uint8_t)(1u << (bit_num & 7));
    return old;
}
int64_t HC_Btr(void* bit_field, int64_t bit_num) {
    uint8_t* p = (uint8_t*)bit_field;
    int64_t old = (p[bit_num >> 3] >> (bit_num & 7)) & 1;
    p[bit_num >> 3] &= (uint8_t)~(1u << (bit_num & 7));
    return old;
}
int64_t HC_Btc(void* bit_field, int64_t bit_num) {
    uint8_t* p = (uint8_t*)bit_field;
    int64_t old = (p[bit_num >> 3] >> (bit_num & 7)) & 1;
    p[bit_num >> 3] ^= (uint8_t)(1u << (bit_num & 7));
    return old;
}
int64_t HC_BCnt(int64_t v) { return (int64_t)__builtin_popcountll((uint64_t)v); }

/* ============================================================== time/sys */

double HC_tS(void) { return now_seconds() - hc_t0; }

int64_t HC_Now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int64_t days = ts.tv_sec / 86400 + 719528; /* days since 1/1/0 */
    int64_t secs = ts.tv_sec % 86400;
    uint64_t frac = (uint64_t)(((double)secs + (double)ts.tv_nsec * 1e-9) * 4294967296.0 / 86400.0);
    return (days << 32) | (int64_t)(frac & 0xFFFFFFFF);
}

void HC_Sleep(int64_t mS) {
    struct timespec ts;
    ts.tv_sec = mS / 1000;
    ts.tv_nsec = (mS % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void HC_Exit(int64_t code) {
    fflush(stdout);
    exit((int)code);
}

void HC_Call(void* addr) {
    if (addr) ((void (*)(void))addr)();
}

/* ================================================================= files */

void* HC_FileRead(char* name, int64_t* size_out) {
    if (size_out) *size_out = 0;
    FILE* f = fopen(name, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)HC_MAlloc(n + 1);
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = 0; /* convenient for text */
    if (size_out) *size_out = (int64_t)rd;
    return buf;
}

int64_t HC_FileWrite(char* name, void* buf, int64_t size) {
    FILE* f = fopen(name, "wb");
    if (!f) return 0;
    size_t wr = fwrite(buf, 1, (size_t)size, f);
    fclose(f);
    return (int64_t)wr;
}

/* ============================================================ exceptions */
/* try{}catch{} frames are allocas in generated code:
 *   [0]      struct HCTryFrame* prev
 *   [16..]   jmp_buf                                                     */

typedef struct HCTryFrame {
    struct HCTryFrame* prev;
    char pad[8];
    jmp_buf jb;
} HCTryFrame;

_Static_assert(sizeof(HCTryFrame) <= HC_TRY_FRAME_SIZE, "try frame too big");

static HCTryFrame* hc_try_top;

void* HC_TryPush(void* frame) {
    HCTryFrame* f = (HCTryFrame*)frame;
    f->prev = hc_try_top;
    hc_try_top = f;
    return f->jb;
}

void HC_TryPop(void) {
    if (hc_try_top) hc_try_top = hc_try_top->prev;
}

void HC_Throw(int64_t ch) {
    HC_Fs->except_ch = ch;
    HC_Fs->except_callers[0] = (int64_t)__builtin_return_address(0);
    HCTryFrame* f = hc_try_top;
    if (!f) {
        char pretty[9] = {0};
        memcpy(pretty, &ch, 8);
        fflush(stdout);
        fprintf(stderr, "Unhandled exception:'%s'\n", pretty);
        exit(1);
    }
    hc_try_top = f->prev;
    longjmp(f->jb, 1);
}

void HC_CatchEnter(void) { HC_Fs->catch_except = 0; }

/* "set Fs->catch_except to TRUE if you want to terminate the search
 * for a hndlr" -- so by default the exception keeps propagating.        */
void HC_CatchDone(void) {
    if (!HC_Fs->catch_except) HC_Throw(HC_Fs->except_ch);
    HC_Fs->catch_except = 0;
}

void HC_PutExcept(int64_t catch_it) {
    char pretty[9] = {0};
    int64_t ch = HC_Fs->except_ch;
    memcpy(pretty, &ch, 8);
    HCOut o = {0};
    o.to_stdout = 1;
    char tmp[128];
    int len = snprintf(tmp, sizeof tmp, "Except:'%s' at %016llX\n", pretty,
                       (unsigned long long)HC_Fs->except_callers[0]);
    out_emit(&o, tmp, (size_t)len);
    fflush(stdout);
    if (catch_it) HC_Fs->catch_except = 1;
}

/* ================================================================== #exe */

static HCOut hc_stream = {0};

void HC_StreamPrint(char* fmt, int64_t argc, int64_t* argv) {
    hc_format(&hc_stream, fmt, argc, argv);
}

char* HC_StreamGet(void) {
    char* r = hc_stream.buf ? hc_stream.buf : (char*)calloc(1, 1);
    hc_stream.buf = NULL;
    hc_stream.len = hc_stream.cap = 0;
    return r; /* caller frees with free() */
}
