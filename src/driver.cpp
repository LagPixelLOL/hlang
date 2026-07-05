// hlang -- hcc driver: front end -> codegen -> JIT or AOT.
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "aot.hpp"
#include "codegen.hpp"
#include "jit.hpp"
#include "lexer.hpp"
#include "parser.hpp"

namespace hc {

static std::string exeDir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
    std::string s(buf);
    auto pos = s.find_last_of('/');
    return pos == std::string::npos ? "." : s.substr(0, pos);
}

static bool fileExists(const std::string& p) { return access(p.c_str(), R_OK) == 0; }

static std::string findPrelude() {
    std::string next = exeDir() + "/lib/HolyC.HH";
    if (fileExists(next)) return next;
#ifdef HCC_DEV_LIB_DIR
    std::string dev = std::string(HCC_DEV_LIB_DIR) + "/HolyC.HH";
    if (fileExists(dev)) return dev;
#endif
    return "";
}

static std::string findRuntimeArchive() {
    std::string next = exeDir() + "/libhcrt.a";
    if (fileExists(next)) return next;
    return "";
}

int compileMain(int argc, char** argv) {
    std::string input, outExe, outObj;
    bool emitLL = false, noPrelude = false;
    int optLevel = 0;
    std::vector<std::string> includeDirs;
    std::vector<char*> progArgs;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--") {
            for (int j = i + 1; j < argc; j++) progArgs.push_back(argv[j]);
            break;
        } else if (a == "-o" && i + 1 < argc) {
            outExe = argv[++i];
        } else if (a == "-c" && i + 1 < argc) {
            outObj = argv[++i];
        } else if (a == "-I" && i + 1 < argc) {
            includeDirs.push_back(argv[++i]);
        } else if (a == "--emit-llvm") {
            emitLL = true;
        } else if (a == "--no-prelude") {
            noPrelude = true;
        } else if (a.size() == 3 && a[0] == '-' && a[1] == 'O') {
            optLevel = a[2] - '0';
        } else if (!a.empty() && a[0] == '-') {
            fprintf(stderr, "hcc: unknown option '%s'\n", a.c_str());
            return 2;
        } else if (input.empty()) {
            input = a;
        } else {
            fprintf(stderr, "hcc: multiple input files\n");
            return 2;
        }
    }
    if (input.empty()) {
        fprintf(stderr, "hcc: no input file\n");
        return 2;
    }

    bool aotMode = !outExe.empty() || !outObj.empty();
    bool jitMode = !aotMode;

    std::string prelude = noPrelude ? "" : findPrelude();
    if (!noPrelude && prelude.empty()) {
        fprintf(stderr, "hcc: cannot find stdlib prelude lib/HolyC.HH\n");
        return 2;
    }

    Lexer lx(input, jitMode, includeDirs);
    if (lx.hadError()) return 1;
    if (!prelude.empty() && !lx.includeFirst(prelude)) {
        fprintf(stderr, "hcc: cannot open prelude '%s'\n", prelude.c_str());
        return 1;
    }
    // #exe{} runs HolyC at compile time via the in-process JIT
    lx.setExeHook([&](const std::string& src, const SrcLoc& loc) -> std::string {
        bool ok = false;
        std::string out = runExeSnippet(src, loc.str() + ":<#exe>", prelude, includeDirs, &ok);
        if (!ok) fprintf(stderr, "%s: error: #exe{} block failed\n", loc.str().c_str());
        return out;
    });

    Parser parser(lx);
    auto prog = parser.parseProgram();
    if (parser.hadError()) return 1;

    CodegenResult cg = codegen(*prog, input, aotMode);
    if (!cg.ok) return 1;
    if (optLevel > 0) optimizeModule(*cg.module, optLevel);

    if (emitLL) {
        std::string s;
        llvm::raw_string_ostream os(s);
        cg.module->print(os, nullptr);
        fputs(os.str().c_str(), stdout);
        if (!aotMode) return 0;
    }

    if (aotMode) {
        std::string obj = outObj;
        if (obj.empty()) obj = outExe + ".o";
        if (!emitObjectFile(*cg.module, obj)) return 1;
        if (!outExe.empty()) {
            std::string rt = findRuntimeArchive();
            if (rt.empty()) {
                fprintf(stderr, "hcc: cannot find libhcrt.a next to hcc\n");
                return 1;
            }
            if (!linkExecutable(obj, outExe, rt)) return 1;
            if (outObj.empty()) remove(obj.c_str());
        }
        return 0;
    }

    // JIT: program argv = [script, args...]
    std::vector<char*> hcArgv;
    hcArgv.push_back(const_cast<char*>(input.c_str()));
    for (char* p : progArgs) hcArgv.push_back(p);
    return runJIT(std::move(cg), (int64_t)hcArgv.size(), hcArgv.data());
}

}  // namespace hc
