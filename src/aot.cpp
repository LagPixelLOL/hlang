// hlang -- AOT compilation to native objects/executables.
#include "aot.hpp"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

#include <cstdio>
#include <cstdlib>

using namespace llvm;

namespace hc {

void optimizeModule(llvm::Module& m, int level) {
    if (level <= 0) return;
    LoopAnalysisManager lam;
    FunctionAnalysisManager fam;
    CGSCCAnalysisManager cgam;
    ModuleAnalysisManager mam;
    PassBuilder pb;
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    OptimizationLevel ol = level == 1   ? OptimizationLevel::O1
                           : level == 2 ? OptimizationLevel::O2
                                        : OptimizationLevel::O3;
    ModulePassManager mpm = pb.buildPerModuleDefaultPipeline(ol);
    mpm.run(m, mam);
}

bool emitObjectFile(llvm::Module& m, const std::string& path) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    // no InitializeNativeTargetAsmParser(): hcc rejects asm{} blocks and
    // emits no module-level inline asm, so the MC asm parser (and its
    // static-initializer cost) stays out of the binary

    std::string triple = sys::getDefaultTargetTriple();
    std::string err;
    const Target* target = TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        fprintf(stderr, "hcc: %s\n", err.c_str());
        return false;
    }
    TargetOptions opts;
    auto* tm = target->createTargetMachine(Triple(triple), "generic", "", opts, Reloc::PIC_);
    m.setDataLayout(tm->createDataLayout());
    m.setTargetTriple(Triple(triple));

    std::error_code ec;
    raw_fd_ostream out(path, ec, sys::fs::OF_None);
    if (ec) {
        fprintf(stderr, "hcc: cannot write '%s': %s\n", path.c_str(), ec.message().c_str());
        return false;
    }
    legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, out, nullptr, CodeGenFileType::ObjectFile)) {
        fprintf(stderr, "hcc: target cannot emit object files\n");
        return false;
    }
    pm.run(m);
    out.flush();
    return true;
}

bool linkExecutable(const std::string& objPath, const std::string& outPath,
                    const std::string& runtimeArchive) {
    // Use the system C compiler as the linker driver (crt files, libc, libm).
    const char* cc = getenv("HCC_CC");
    std::string drivers[] = {cc ? cc : "", "cc", "gcc", "clang"};
    for (const std::string& d : drivers) {
        if (d.empty()) continue;
        std::string probe = "command -v " + d + " >/dev/null 2>&1";
        if (system(probe.c_str()) != 0) continue;
        std::string cmd =
            d + " -o '" + outPath + "' '" + objPath + "' '" + runtimeArchive + "' -lm";
        int rc = system(cmd.c_str());
        if (rc != 0) {
            fprintf(stderr, "hcc: link failed: %s\n", cmd.c_str());
            return false;
        }
        return true;
    }
    fprintf(stderr, "hcc: no C compiler found for linking (set HCC_CC)\n");
    return false;
}

}  // namespace hc
