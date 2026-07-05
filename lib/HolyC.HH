// HolyC.HH -- hlang stdlib prelude (KernelA.HH spirit).
// Auto-included by hcc before your program. Binds the hcrt runtime to the
// TempleOS names via _extern.

#define TRUE  1
#define FALSE 0
#define NULL  0
#define ON    1
#define OFF   0
#define Bool  U8

#define I8_MIN  (-0x80)
#define I8_MAX  0x7F
#define U8_MAX  0xFF
#define I16_MIN (-0x8000)
#define I16_MAX 0x7FFF
#define U16_MAX 0xFFFF
#define I32_MIN (-0x80000000)
#define I32_MAX 0x7FFFFFFF
#define U32_MAX 0xFFFFFFFF
#define I64_MIN (-0x7FFFFFFFFFFFFFFF-1)
#define I64_MAX 0x7FFFFFFFFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFF

#define pi        3.14159265358979323
#define log2_10   3.32192809488736235
#define log10_2   0.30102999566398120
#define loge_2    0.69314718055994531

// ------------------------------------------------------------ task
// The current task. throw() char codes land in Fs->except_ch; set
// Fs->catch_except=TRUE inside catch{} to terminate the handler search.
class CTask
{
  I64 except_ch;
  I64 catch_except;
  I64 except_callers[8];
  U8  *task_name;
};

_extern HC_Fs CTask *Fs;

// ------------------------------------------------------------ output
_extern HC_Print    U0 Print(U8 *fmt,...);
_extern HC_PutChars U0 PutChars(I64 chars);
_extern HC_PutChar  U0 PutChar(I64 ch);
_extern HC_PutS     U0 PutS(U8 *st);
_extern HC_StrPrint     U8 *StrPrint(U8 *dst,U8 *fmt,...);
_extern HC_MStrPrint    U8 *MStrPrint(U8 *fmt,...);
_extern HC_CatPrint     U8 *CatPrint(U8 *dst,U8 *fmt,...);
_extern HC_StrPrintJoin U8 *StrPrintJoin(U8 *dst,U8 *fmt,I64 argc,I64 *argv);

// ------------------------------------------------------------ input
_extern HC_GetStr   U8 *GetStr(U8 *msg=NULL);
_extern HC_GetChar  I64 GetChar();
_extern HC_GetI64   I64 GetI64(U8 *msg=NULL,I64 dft=0,
        I64 min=I64_MIN,I64 max=I64_MAX);
_extern HC_GetF64   F64 GetF64(U8 *msg=NULL,F64 dft=0.0);
_extern HC_YorN     Bool YorN();
_extern HC_PressAKey U0 PressAKey();

// ------------------------------------------------------------ memory
_extern HC_MAlloc     U0 *MAlloc(I64 size);
_extern HC_CAlloc     U0 *CAlloc(I64 size);
_extern HC_MAllocIdent U0 *MAllocIdent(U0 *src);
_extern HC_Free       U0 Free(U0 *addr);
_extern HC_MSize      I64 MSize(U0 *src);
_extern HC_MemCpy     U0 *MemCpy(U0 *dst,U0 *src,I64 cnt);
_extern HC_MemSet     U0 *MemSet(U0 *dst,I64 val,I64 cnt);
_extern HC_MemCmp     I64 MemCmp(U0 *ptr1,U0 *ptr2,I64 cnt);

// ------------------------------------------------------------ strings
_extern HC_StrLen   I64 StrLen(U8 *st);
_extern HC_StrCpy   U8 *StrCpy(U8 *dst,U8 *src);
_extern HC_StrNew   U8 *StrNew(U8 *st);
_extern HC_StrCmp   I64 StrCmp(U8 *st1,U8 *st2);
_extern HC_StrNCmp  I64 StrNCmp(U8 *st1,U8 *st2,I64 n);
_extern HC_StrICmp  I64 StrICmp(U8 *st1,U8 *st2);
_extern HC_StrFind  U8 *StrFind(U8 *needle,U8 *haystack);
_extern HC_Str2I64  I64 Str2I64(U8 *st,I64 radix=10);
_extern HC_Str2F64  F64 Str2F64(U8 *st);

// ------------------------------------------------------------ math
_extern HC_Abs    F64 Abs(F64 d);
_extern HC_Sqrt   F64 Sqrt(F64 d);
_extern HC_Sin    F64 Sin(F64 d);
_extern HC_Cos    F64 Cos(F64 d);
_extern HC_Tan    F64 Tan(F64 d);
_extern HC_ASin   F64 ASin(F64 d);
_extern HC_ACos   F64 ACos(F64 d);
_extern HC_ATan   F64 ATan(F64 d);
_extern HC_Arg    F64 Arg(F64 x,F64 y);
_extern HC_Ln     F64 Ln(F64 d);
_extern HC_Log2   F64 Log2(F64 d);
_extern HC_Log10  F64 Log10(F64 d);
_extern HC_Exp    F64 Exp(F64 d);
_extern HC_Pow    F64 Pow(F64 base,F64 e);
_extern HC_Floor  F64 Floor(F64 d);
_extern HC_Ceil   F64 Ceil(F64 d);
_extern HC_Round  F64 Round(F64 d);
_extern HC_Trunc  F64 Trunc(F64 d);
_extern HC_AbsI64  I64 AbsI64(I64 i);
_extern HC_MinI64  I64 MinI64(I64 n1,I64 n2);
_extern HC_MaxI64  I64 MaxI64(I64 n1,I64 n2);
_extern HC_SignI64 I64 SignI64(I64 i);
_extern HC_SqrI64  I64 SqrI64(I64 i);

// ------------------------------------------------------------ random
_extern HC_Seed    U0 Seed(I64 seed=0);
_extern HC_RandU64 U64 RandU64();
_extern HC_RandI64 I64 RandI64();
_extern HC_RandU32 U32 RandU32();
_extern HC_RandU16 U16 RandU16();

// ------------------------------------------------------------ bits
_extern HC_Bt   Bool Bt(U0 *bit_field,I64 bit_num);
_extern HC_Bts  Bool Bts(U0 *bit_field,I64 bit_num);
_extern HC_Btr  Bool Btr(U0 *bit_field,I64 bit_num);
_extern HC_Btc  Bool Btc(U0 *bit_field,I64 bit_num);
_extern HC_BCnt I64 BCnt(I64 d);

// ------------------------------------------------------------ time/system
_extern HC_tS     F64 tS();
_extern HC_Now    I64 Now();          //CDate:days since 1/1/0 <<32 | frac
_extern HC_Sleep  U0 Sleep(I64 mS);
_extern HC_Exit   U0 Exit(I64 code=0);
_extern HC_Call   U0 Call(U0 *addr);
_extern HC_ArgCnt I64 ArgCnt();       //host cmd line args (hosted extra)
_extern HC_ArgStr U8 *ArgStr(I64 i);

// ------------------------------------------------------------ files
_extern HC_FileRead  U0 *FileRead(U8 *filename,I64 *size=NULL);
_extern HC_FileWrite I64 FileWrite(U8 *filename,U0 *fbuf,I64 size);

// ------------------------------------------------------------ exceptions
// "throw is a function with an 8-byte or less char arg."
_extern HC_Throw     U0 throw(I64 ch=0);
_extern HC_PutExcept U0 PutExcept(Bool catch_it=TRUE);

// ------------------------------------------------------------ #exe support
_extern HC_StreamPrint U0 StreamPrint(U8 *fmt,...);

// ------------------------------------------------------------ HolyC-level
// A few library routines written in HolyC itself, TempleOS style.
U8 ToUpper(U8 ch)
{
  if ('a'<=ch<='z')
    return ch-0x20;
  return ch;
}

U8 ToLower(U8 ch)
{
  if ('A'<=ch<='Z')
    return ch+0x20;
  return ch;
}

I64 StrOcc(U8 *src,I64 ch)
{//Count occurrences of a char in a string.
  I64 cnt=0;
  if (!src) return 0;
  while (*src) {
    if (*src==ch)
      cnt++;
    src++;
  }
  return cnt;
}

U0 SwapI64(I64 *num1,I64 *num2)
{
  I64 tmp=*num1;
  *num1=*num2;
  *num2=tmp;
}

I64 ClampI64(I64 num,I64 lo,I64 hi)
{
  if (num<lo) return lo;
  if (num>hi) return hi;
  return num;
}

// ------------------------------------------------------------ options
// TempleOS compiler options; accepted as no-ops in hcc.
#define OPTf_ECHO                 0
#define OPTf_TRACE                1
#define OPTf_PSTACK_SIZE          2
#define OPTf_NO_REG_VAR           3
#define OPTf_WARN_PAREN           4
#define OPTf_WARN_DUP_TYPES       5
#define OPTf_WARN_UNUSED_VAR      6
#define OPTf_WARN_HEADER_MISMATCH 7
#define OPTf_EXTERNS_TO_IMPORTS   8
#define OPTf_KEEP_PRIVATE         9
#define OPTf_GLBLS_ON_DATA_HEAP   10
#define OPTf_NO_BUILTIN_CONST     11

I64 Option(I64 opt_num,I64 val)
{//Compiler option get/set. hcc: parsed for compat, has no effect.
  no_warn opt_num;
  no_warn val;
  return 0;
}
