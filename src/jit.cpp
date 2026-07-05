// hlang -- JIT execution via LLVM ORC (LLJIT).
//
// JIT is the native mode of HolyC: "There is no main() function. Any code
// outside of functions gets executed upon start-up, in order."
#include "jit.hpp"

#include <llvm/ExecutionEngine/Orc/AbsoluteSymbols.h>
#include <llvm/ExecutionEngine/Orc/ExecutorProcessControl.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/TargetSelect.h>
#include <setjmp.h>

#include <cstdio>

#include "../runtime/hcrt.h"
#include "lexer.hpp"
#include "parser.hpp"

using namespace llvm;
using namespace llvm::orc;

namespace hc {

static void initTargets() {
    static bool done = false;
    if (done) return;
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    // no InitializeNativeTargetAsmParser(): hcc rejects asm{} blocks and
    // emits no module-level inline asm, so the MC asm parser (and its
    // static-initializer cost) stays out of the binary
    done = true;
}

// Register the hcrt symbols (statically linked into hcc) with the JIT so
// generated code binds to the in-process runtime -- fully self-contained.
static void defineRuntimeSymbols(LLJIT& jit) {
    SymbolMap m;
    auto add = [&](const char* name, void* p) {
        m[jit.mangleAndIntern(name)] = ExecutorSymbolDef(
            ExecutorAddr::fromPtr(p), JITSymbolFlags::Exported | JITSymbolFlags::Callable);
    };
#define RT(fn) add(#fn, (void*)&fn)
    RT(HC_Print);
    RT(HC_PutChars);
    RT(HC_PutChar);
    RT(HC_PutS);
    RT(HC_StrPrint);
    RT(HC_MStrPrint);
    RT(HC_CatPrint);
    RT(HC_StrPrintJoin);
    RT(HC_GetStr);
    RT(HC_GetChar);
    RT(HC_GetI64);
    RT(HC_GetF64);
    RT(HC_YorN);
    RT(HC_PressAKey);
    RT(HC_MAlloc);
    RT(HC_CAlloc);
    RT(HC_MAllocIdent);
    RT(HC_Free);
    RT(HC_MSize);
    RT(HC_MemCpy);
    RT(HC_MemSet);
    RT(HC_MemCmp);
    RT(HC_StrLen);
    RT(HC_StrCpy);
    RT(HC_StrNew);
    RT(HC_StrCmp);
    RT(HC_StrNCmp);
    RT(HC_StrICmp);
    RT(HC_StrFind);
    RT(HC_Str2I64);
    RT(HC_Str2F64);
    RT(HC_Abs);
    RT(HC_Sqrt);
    RT(HC_Sin);
    RT(HC_Cos);
    RT(HC_Tan);
    RT(HC_ASin);
    RT(HC_ACos);
    RT(HC_ATan);
    RT(HC_Arg);
    RT(HC_Ln);
    RT(HC_Log2);
    RT(HC_Log10);
    RT(HC_Exp);
    RT(HC_Pow);
    RT(HC_PowI64);
    RT(HC_Floor);
    RT(HC_Ceil);
    RT(HC_Round);
    RT(HC_Trunc);
    RT(HC_AbsI64);
    RT(HC_MinI64);
    RT(HC_MaxI64);
    RT(HC_SignI64);
    RT(HC_SqrI64);
    RT(HC_Seed);
    RT(HC_RandU64);
    RT(HC_RandI64);
    RT(HC_RandU32);
    RT(HC_RandU16);
    RT(HC_Bt);
    RT(HC_Bts);
    RT(HC_Btr);
    RT(HC_Btc);
    RT(HC_BCnt);
    RT(HC_tS);
    RT(HC_Now);
    RT(HC_Sleep);
    RT(HC_Exit);
    RT(HC_Call);
    RT(HC_ArgCnt);
    RT(HC_ArgStr);
    RT(HC_FileRead);
    RT(HC_FileWrite);
    RT(HC_Throw);
    RT(HC_TryPush);
    RT(HC_TryPop);
    RT(HC_CatchEnter);
    RT(HC_CatchDone);
    RT(HC_PutExcept);
    RT(HC_StreamPrint);
    RT(HC_RtInit);
#undef RT
    add("HC_Fs", (void*)&HC_Fs);
    add("setjmp", (void*)&setjmp);
    // llvm.memcpy/memset lower to libc calls
    add("memcpy", (void*)&memcpy);
    add("memset", (void*)&memset);
    add("memmove", (void*)&memmove);
    cantFail(jit.getMainJITDylib().define(absoluteSymbols(std::move(m))));
}

static std::unique_ptr<LLJIT> makeJit(CodegenResult& cg, int optLevel, bool* ok) {
    *ok = false;
    initTargets();
    // Set the JIT backend codegen opt level to match -O (affects instruction
    // selection / regalloc quality even when the IR pipeline didn't run).
    CodeGenOptLevel cgLevel = optLevel >= 3   ? CodeGenOptLevel::Aggressive
                              : optLevel == 2 ? CodeGenOptLevel::Default
                              : optLevel == 1 ? CodeGenOptLevel::Less
                                              : CodeGenOptLevel::None;
    LLJITBuilder builder;
    auto jtmb = JITTargetMachineBuilder::detectHost();
    if (jtmb) {
        jtmb->setCodeGenOptLevel(cgLevel);
        builder.setJITTargetMachineBuilder(std::move(*jtmb));
    } else {
        consumeError(jtmb.takeError());
    }
    auto jitOr = builder.create();
    if (!jitOr) {
        errs() << "hcc: cannot create JIT: " << toString(jitOr.takeError()) << "\n";
        return nullptr;
    }
    std::unique_ptr<LLJIT> jit = std::move(*jitOr);
    // fall back to process symbols (libc) for anything else
    auto gen =
        DynamicLibrarySearchGenerator::GetForCurrentProcess(jit->getDataLayout().getGlobalPrefix());
    if (gen)
        jit->getMainJITDylib().addGenerator(std::move(*gen));
    else
        consumeError(gen.takeError());
    defineRuntimeSymbols(*jit);
    ThreadSafeModule tsm(std::move(cg.module), std::move(cg.ctx));
    if (Error e = jit->addIRModule(std::move(tsm))) {
        errs() << "hcc: JIT error: " << toString(std::move(e)) << "\n";
        return nullptr;
    }
    *ok = true;
    return jit;
}

int runJIT(CodegenResult cg, int64_t argc, char** argv, int optLevel) {
    bool ok = false;
    auto jit = makeJit(cg, optLevel, &ok);
    if (!ok) return 1;
    auto sym = jit->lookup("__HC_startup");
    if (!sym) {
        errs() << "hcc: " << toString(sym.takeError()) << "\n";
        return 1;
    }
    HC_RtInit(argc, argv);
    auto* fn = sym->toPtr<void (*)()>();
    fn();
    fflush(stdout);
    return 0;
}

std::string runExeSnippet(const std::string& src, const std::string& name,
                          const std::string& preludePath,
                          const std::vector<std::string>& includeDirs, bool* ok) {
    *ok = false;
    std::string buf = src;
    if (!preludePath.empty()) buf = "#include \"" + preludePath + "\"\n" + buf;
    auto lx = Lexer::fromBuffer(buf, name, /*jitMode=*/true);
    Parser parser(*lx);
    auto prog = parser.parseProgram();
    if (parser.hadError()) return "";
    CodegenResult cg = codegen(*prog, name, /*aotMode=*/false);
    if (!cg.ok) return "";
    bool jok = false;
    auto jit = makeJit(cg, /*optLevel=*/0, &jok);  // compile-time snippets: fast
    if (!jok) return "";
    auto sym = jit->lookup("__HC_startup");
    if (!sym) {
        errs() << "hcc: #exe: " << toString(sym.takeError()) << "\n";
        return "";
    }
    free(HC_StreamGet());  // reset stream
    auto* fn = sym->toPtr<void (*)()>();
    fn();
    char* out = HC_StreamGet();
    std::string r = out;
    free(out);
    *ok = true;
    return r;
}

}  // namespace hc
