// hcc -- HolyC compiler driver.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "lexer.hpp"
#include "parser.hpp"

namespace hc {
int compileMain(int argc, char** argv);  // implemented as we go
}

static void usage() {
    fprintf(stderr,
            "usage: hcc [options] file.HC [-- args...]\n"
            "  (default)          JIT compile and run\n"
            "  -o <file>          AOT compile and link an executable\n"
            "  -c <file.o>        AOT compile to an object file\n"
            "  --emit-llvm        print LLVM IR\n"
            "  --dump-tokens      dump lexer tokens\n"
            "  --dump-ast         dump parse tree\n"
            "  -I <dir>           add include search dir\n"
            "  --no-prelude       don't auto-include the HolyC stdlib prelude\n");
}

static int dumpTokens(const std::string& path) {
    hc::Lexer lx(path, /*jitMode=*/true, {});
    for (;;) {
        hc::Token t = lx.next();
        if (t.kind == hc::Tok::Eof) break;
        switch (t.kind) {
            case hc::Tok::Ident:
                printf("%d:%d\tident\t%s\n", t.loc.line, t.loc.col, t.text.c_str());
                break;
            case hc::Tok::IntLit:
                printf("%d:%d\tint\t%lld\n", t.loc.line, t.loc.col, (long long)t.ival);
                break;
            case hc::Tok::FloatLit:
                printf("%d:%d\tfloat\t%g\n", t.loc.line, t.loc.col, t.fval);
                break;
            case hc::Tok::StrLit: {
                std::string esc;
                for (char c : t.text) {
                    if (c == '\n')
                        esc += "\\n";
                    else if (c == '\t')
                        esc += "\\t";
                    else if (c == '\0')
                        esc += "\\0";
                    else
                        esc += c;
                }
                printf("%d:%d\tstr\t\"%s\"\n", t.loc.line, t.loc.col, esc.c_str());
                break;
            }
            case hc::Tok::CharLit:
                printf("%d:%d\tchar\t%lld '%s'\n", t.loc.line, t.loc.col, (long long)t.ival,
                       t.text.c_str());
                break;
            case hc::Tok::Punct:
                printf("%d:%d\tpunct\t%s\n", t.loc.line, t.loc.col, hc::punctStr(t.punct));
                break;
            default:
                break;
        }
    }
    return lx.hadError() ? 1 : 0;
}

static int dumpAst(const std::string& path) {
    hc::Lexer lx(path, /*jitMode=*/true, {});
    hc::Parser parser(lx);
    auto prog = parser.parseProgram();
    fputs(hc::dumpProgram(*prog).c_str(), stdout);
    return parser.hadError() ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    // token/ast-dump fast paths (front-end testing)
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dump-tokens") || !strcmp(argv[i], "--dump-ast")) {
            bool ast = !strcmp(argv[i], "--dump-ast");
            for (int j = 1; j < argc; j++)
                if (argv[j][0] != '-') return ast ? dumpAst(argv[j]) : dumpTokens(argv[j]);
            usage();
            return 2;
        }
    }
    return hc::compileMain(argc, argv);
}
