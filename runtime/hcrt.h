/* hlang -- HolyC runtime (hcrt).
 *
 * Backs the HolyC stdlib in the spirit of the TempleOS kernel library.
 * Exported symbols carry the HC_ prefix; lib/HolyC.HH binds them to their
 * TempleOS names via _extern.
 *
 * HolyC variadic convention: fn(named..., I64 argc, I64 *argv) -- every
 * vararg occupies one 64-bit slot (F64 args are bit-cast), exactly like
 * TempleOS pushing 8-byte args on the stack.
 */
#ifndef HCRT_H
#define HCRT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------- task (Fs) */
typedef struct HCTask {
    int64_t except_ch;          /* 8-byte char passed to throw()          */
    int64_t catch_except;       /* set TRUE in catch{} to stop the search */
    int64_t except_callers[8];  /* return addrs at throw()                */
    char*   task_name;
} HCTask;

extern HCTask* HC_Fs;

/* ------------------------------------------------------------- startup   */
void HC_RtInit(int64_t argc, char** argv);

/* ------------------------------------------------------------- output    */
void HC_Print(char* fmt, int64_t argc, int64_t* argv);
void HC_PutChars(int64_t chars);           /* up to 8 packed chars */
void HC_PutChar(int64_t ch);
void HC_PutS(char* st);
char* HC_StrPrint(char* dst, char* fmt, int64_t argc, int64_t* argv);
char* HC_MStrPrint(char* fmt, int64_t argc, int64_t* argv);
char* HC_CatPrint(char* dst, char* fmt, int64_t argc, int64_t* argv);
char* HC_StrPrintJoin(char* dst, char* fmt, int64_t argc, int64_t* argv);

/* ------------------------------------------------------------- input     */
char* HC_GetStr(char* msg);                /* MAlloc'ed line               */
int64_t HC_GetChar(void);
int64_t HC_GetI64(char* msg, int64_t dft, int64_t min, int64_t max);
double HC_GetF64(char* msg, double dft);
int64_t HC_YorN(void);
void HC_PressAKey(void);

/* ------------------------------------------------------------- memory    */
void* HC_MAlloc(int64_t size);
void* HC_CAlloc(int64_t size);
void* HC_MAllocIdent(void* src);
void HC_Free(void* p);                     /* NULL ok                      */
int64_t HC_MSize(void* p);
void* HC_MemCpy(void* dst, void* src, int64_t n);
void* HC_MemSet(void* dst, int64_t val, int64_t n);
int64_t HC_MemCmp(void* a, void* b, int64_t n);

/* ------------------------------------------------------------- strings   */
int64_t HC_StrLen(char* s);
char* HC_StrCpy(char* dst, char* src);
char* HC_StrNew(char* s);
int64_t HC_StrCmp(char* a, char* b);
int64_t HC_StrNCmp(char* a, char* b, int64_t n);
int64_t HC_StrICmp(char* a, char* b);
char* HC_StrFind(char* needle, char* haystack);
int64_t HC_Str2I64(char* s, int64_t radix);
double HC_Str2F64(char* s);

/* ------------------------------------------------------------- math      */
double HC_Abs(double x);
double HC_Sqrt(double x);
double HC_Sin(double x);
double HC_Cos(double x);
double HC_Tan(double x);
double HC_ASin(double x);
double HC_ACos(double x);
double HC_ATan(double x);
double HC_Arg(double x, double y);         /* angle of (x,y), TempleOS Arg */
double HC_Ln(double x);
double HC_Log2(double x);
double HC_Log10(double x);
double HC_Exp(double x);
double HC_Pow(double base, double e);
int64_t HC_PowI64(int64_t base, int64_t e);
double HC_Floor(double x);
double HC_Ceil(double x);
double HC_Round(double x);
double HC_Trunc(double x);
int64_t HC_AbsI64(int64_t x);
int64_t HC_MinI64(int64_t a, int64_t b);
int64_t HC_MaxI64(int64_t a, int64_t b);
int64_t HC_SignI64(int64_t x);
int64_t HC_SqrI64(int64_t x);

/* ------------------------------------------------------------- random    */
void HC_Seed(int64_t seed);                /* 0 => time-based              */
int64_t HC_RandU64(void);
int64_t HC_RandI64(void);
int64_t HC_RandU32(void);
int64_t HC_RandU16(void);

/* ------------------------------------------------------------- bits      */
int64_t HC_Bt(void* bit_field, int64_t bit_num);
int64_t HC_Bts(void* bit_field, int64_t bit_num);
int64_t HC_Btr(void* bit_field, int64_t bit_num);
int64_t HC_Btc(void* bit_field, int64_t bit_num);
int64_t HC_BCnt(int64_t v);                /* popcount                     */

/* ------------------------------------------------------------- time/sys  */
double HC_tS(void);                        /* seconds since start          */
int64_t HC_Now(void);                      /* CDate: days<<32 | frac       */
void HC_Sleep(int64_t mS);
void HC_Exit(int64_t code);
void HC_Call(void* addr);                  /* call code at addr, no args   */
int64_t HC_ArgCnt(void);                   /* host cmd line: count         */
char* HC_ArgStr(int64_t i);                /* host cmd line: arg i         */

/* ------------------------------------------------------------- files     */
void* HC_FileRead(char* name, int64_t* size_out);
int64_t HC_FileWrite(char* name, void* buf, int64_t size);

/* ------------------------------------------------------------- except    */
void HC_Throw(int64_t ch);
void* HC_TryPush(void* frame);             /* returns jmp_buf ptr          */
void HC_TryPop(void);
void HC_CatchEnter(void);
void HC_CatchDone(void);                   /* rethrows unless catch_except */
void HC_PutExcept(int64_t catch_it);

/* how much space codegen must reserve for a try frame */
#define HC_TRY_FRAME_SIZE 256

/* ------------------------------------------------------------- #exe      */
void HC_StreamPrint(char* fmt, int64_t argc, int64_t* argv);
char* HC_StreamGet(void);                  /* compiler-side: take & reset  */

#ifdef __cplusplus
}
#endif
#endif /* HCRT_H */
